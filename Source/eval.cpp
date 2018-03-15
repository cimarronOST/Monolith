/*
  Monolith 0.4  Copyright (C) 2017 Jonas Mayr

  This file is part of Monolith.

  Monolith is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Monolith is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with Monolith. If not, see <http://www.gnu.org/licenses/>.
*/


#include <array>

#include "stream.h"
#include "engine.h"
#include "magic.h"
#include "attack.h"
#include "bit.h"
#include "eval.h"

namespace
{
	static_assert(WHITE == 0, "index");
	static_assert(BLACK == 1, "index");
	static_assert(MG == 0, "index");
	static_assert(EG == 1, "index");

	// filled during evaluation

	pawn pawnhash;
	uint64 all_attacks[2]{ };

	// variables filled at startup

	uint64 adjacent[8]{ };
	uint64 front_span[2][64]{ };
	uint64 king_chain[2][8]{ };

	int phase{ };

	// some constants

	const int push[]{ 8, 56, -8 };
	const int negate[]{ 1, -1 };

	const uint64 is_outpost[]{ 0x3c3c3c000000, 0x3c3c3c0000 };

	const int max_weight
	{
		16 * eval::phase_value[PAWNS] +
		 4 * eval::phase_value[KNIGHTS] +
		 4 * eval::phase_value[BISHOPS] +
		 4 * eval::phase_value[ROOKS] +
		 2 * eval::phase_value[QUEENS]
	};

	struct pinned
	{
		uint64 moves[64];
		uint32 idx[8];
		int cnt;
	} pin;

	std::array<std::string, 9> feature
	{ { "pawns", "knights", "bishops", "rooks", "queens", "kings", "material", "mobility", "passed pawns" } };
}

namespace sum
{
	int scores(int sum[][2][2], game_stage stage)
	{
		// summing up scores of all evaluation features

		auto score{ 0 };
		for (auto f{ 0U }; f < feature.size(); ++f)
			score += sum[f][stage][WHITE] - sum[f][stage][BLACK];
		return score;
	}

	int pieces(const board &pos, int col)
	{
		// summing up phase values of all pieces on the board

		auto sum{ 0 };
		for (int p{ KNIGHTS }; p <= QUEENS; ++p)
			sum += bit::popcnt(pos.pieces[p] & pos.side[col]) * eval::phase_value[p];
		return sum;
	}

	void material(int sum[][2][2])
	{
		// initialising material sum to show correct 'eval' output

		for (int stage{ MG }; stage <= EG; ++stage)
		{
			for (auto &s : sum[MATERIAL][stage])
			{
				s = - 8 * eval::value[stage][PAWNS]
					- 2 * eval::value[stage][KNIGHTS]
					- 2 * eval::value[stage][BISHOPS]
					- 2 * eval::value[stage][ROOKS]
					- 1 * eval::value[stage][QUEENS];
			}
		}
	}
}

namespace pinning
{
	void pin_down(const board &pos, int turn)
	{
		// finding pins for piece mobility evaluation

		pin.cnt = 0;
		auto xturn{ turn ^ 1 };
		auto fr_king{ pos.pieces[KINGS] & pos.side[turn] };
		auto all_att{ (attack::slide_map[ROOK][pos.king_sq[turn]] & pos.side[xturn] & (pos.pieces[ROOKS] | pos.pieces[QUEENS]))
			| (attack::slide_map[BISHOP][pos.king_sq[turn]] & pos.side[xturn] & (pos.pieces[BISHOPS] | pos.pieces[QUEENS])) };

		while (all_att)
		{
			// generating rays centered on the king square

			auto ray_to_att{ attack::by_slider<QUEEN>(pos.king_sq[turn], all_att) };
			auto att{ 1ULL << bit::scan(all_att) };

			if (!(att & ray_to_att))
			{
				all_att &= all_att - 1;
				continue;
			}

			// creating final ray from king to attacker

			assert(fr_king);
			auto x_ray{ 0ULL };
			for (auto dir{ 0 }; dir < 8; ++dir)
			{
				auto flood{ fr_king };
				for (; !(flood & magic::ray[dir].boarder); flood |= bit::shift(flood, magic::ray[dir].shift));

				if (flood & att)
				{
					x_ray = flood & ray_to_att;
					break;
				}
			}

			assert(x_ray & att);
			assert(!(x_ray & fr_king));

			// allowing only moves inside the pinning ray

			if ((x_ray & pos.side[turn]) && bit::popcnt(x_ray & pos.side[BOTH]) == 2)
			{
				assert(bit::popcnt(x_ray & pos.side[turn]) == 1);

				auto sq{ bit::scan(x_ray & pos.side[turn]) };
				pin.moves[sq] = x_ray;
				pin.idx[pin.cnt++] = sq;
			}
			all_att &= all_att - 1;
		}
	}

	void unpin()
	{
		// clearing the pin-table

		if (pin.cnt != 0)
		{
			assert(pin.cnt <= 8);
			for (auto i{ 0 }; i < pin.cnt; ++i)
				pin.moves[pin.idx[i]] = ~0ULL;
		}
	}
}

namespace material
{
	// assessing some material constellations

	bool lone_knights(const board &pos)
	{
		return (pos.pieces[KNIGHTS] | pos.pieces[KINGS]) == pos.side[BOTH];
	}

	bool lone_bishops(const board &pos)
	{
		return (pos.pieces[BISHOPS] | pos.pieces[KINGS]) == pos.side[BOTH];
	}

	bool opposite_bishops(const board &pos)
	{
		return (pos.pieces[BISHOPS] | pos.pieces[PAWNS] | pos.pieces[KINGS]) == pos.side[BOTH]
			&& bit::popcnt(pos.pieces[BISHOPS] & pos.side[WHITE]) == 1
			&& bit::popcnt(pos.pieces[BISHOPS] & pos.side[BLACK]) == 1
			&& (pos.pieces[BISHOPS] & square::white)
			&& (pos.pieces[BISHOPS] & square::black);
	}
}

namespace file
{
	bool open(const board &pos, int sq, int col, int file)
	{
		return !(front_span[col][sq] & pos.pieces[PAWNS] & bit::file[file]);
	}
}

namespace path
{
	uint64 blocked(const board &pos, int sq, int col, int xcol)
	{
		return front_span[col][sq] & pos.pieces[PAWNS] & pos.side[xcol];
	}

	bool is_passed(const board &pos, int sq, int col, int xcol, int file)
	{
		assert(square::file(sq) == file);
		assert(pos.side[col] & (1ULL << sq));
		assert(col == (xcol ^ 1));

		return file::open(pos, sq, col, file) && !blocked(pos, sq, col, xcol);
	}
}

namespace output
{
	void align(double score)
	{
		// aligning all scores to create a itemised table after the "eval" command

		score /= 100.0;
		sync::cout.precision(2);
		sync::cout << std::fixed
			<< (score < 0 ? "" : " ")
			<< (std::abs(score) >= 10 ? "" : " ")
			<< score << " ";
	}
}

namespace king_attack
{
	void add(piece_index p, const board &pos, uint64 &targets, uint64 &king_zone, int &threat_cnt, int &threat_sum)
	{
		// adding attacks of the king for king safety evaluation

		assert(p <= QUEENS);
		auto king_threat{ targets & king_zone };
		if (king_threat)
		{
			threat_cnt += 1;
			threat_sum += eval::threat_value[p] * bit::popcnt(king_threat);
		}
	}
}

void eval::fill_tables()
{
	// pre-filling table of pinned moves

	std::fill(pin.moves, pin.moves + 64, ~0ULL);

	// finishing the piece-square-table

	for (int p{ PAWNS }; p <= KINGS; ++p)
	{
		for (int s{ MG }; s <= EG; ++s)
		{
			for (int sq{ H1 }; sq <= A8; ++sq)
			{
				psqt[WHITE][p][s][63 - sq] += value[s][p];
				psqt[BLACK][p][s][sq] = psqt[WHITE][p][s][63 - sq];
			}
		}
	}

	// mirroring pawn tables

	for (int r{ R1 }; r <= R8; ++r)
	{
		passed_rank[BLACK][r] = passed_rank[WHITE][R8 - r];
		shield_rank[BLACK][r] = shield_rank[WHITE][R8 - r];
		storm_rank[BLACK][r] = storm_rank[WHITE][R8 - r];
	}

	// creating the adjacent-files & king-chain tables

	for (int f{ H }; f <= A; ++f)
	{
		adjacent[f] |= f > H ? bit::file[f - 1] : 0ULL;
		adjacent[f] |= f < A ? bit::file[f + 1] : 0ULL;

		king_chain[WHITE][f] = bit::rank[R1] & (adjacent[f] | (bit::rank[R1] & bit::file[f]));
		king_chain[BLACK][f] = king_chain[WHITE][f] << 56;
	}

	// filling the front span table

	for (int sq{ H1 }; sq <= A8; ++sq)
	{
		auto f{ square::file(sq) };
		auto file_span{ bit::file[f] | adjacent[f] };

		front_span[WHITE][sq] = file_span & attack::in_front[WHITE][sq];
		front_span[BLACK][sq] = file_span & attack::in_front[BLACK][sq];
	}
}

bool eval::obvious_draw(const board &pos)
{
	// recognising positions with insufficient mating material
	// KK, KNK, KBK, KB*KB*, KNNK

	if (material::lone_bishops(pos) && (!(square::white & pos.pieces[BISHOPS]) || !(square::black & pos.pieces[BISHOPS])))
		return true;

	if (material::lone_knights(pos) && (!(pos.pieces[KNIGHTS] & pos.side[WHITE]) || !(pos.pieces[KNIGHTS] & pos.side[BLACK])))
		return true;

	return false;
}

int eval::scale_draw(const board &pos, int score)
{
	// scaling the score for drawish positions

	int winning{ score > 0 ? WHITE : BLACK };

	if (!(pos.side[winning] & pos.pieces[PAWNS])
		&& sum::pieces(pos, winning) - sum::pieces(pos, winning ^ 1) <= phase_value[KNIGHTS])
		score /= 4;

	if (material::opposite_bishops(pos))
		score = score * 3 / 4;

	return score;
}

int eval::interpolate(const board &pos, int sum[][2][2])
{
	// interpolating the MG- and EG-scores

	assert(phase >= 0);
	auto weight{ phase <= max_weight ? phase : max_weight };
	auto mg_score{ sum::scores(sum, MG) };
	auto eg_score{ sum::scores(sum, EG) };

	return (mg_score * weight + eg_score * (max_weight - weight)) / max_weight;
}

void eval::evaluate(const board &pos, int sum[][2][2])
{
	// initialising & starting actual evaluation

	phase = 0;
	all_attacks[WHITE] = 0ULL;
	all_attacks[BLACK] = 0ULL;

	pieces(pos, sum, WHITE);
	pieces(pos, sum, BLACK);
	pawns(pos, sum);
}

int eval::static_eval(const board &pos)
{
	// filtering out obviously drawn positions

	if (obvious_draw(pos))
		return engine::contempt[pos.xturn];

	// evaluating

	int sum[feature.size()][2][2]{ };
	evaluate(pos, sum);
	auto score{ interpolate(pos, sum) };
	assert(abs(score) < MATE_SCORE);

	// scaling drawish positions

	return negate[pos.turn] * scale_draw(pos, score);
}

void eval::itemise_eval(const board &pos)
{
	// itemising the evaluation for debugging

	int sum[feature.size()][2][2]{ };
	sum::material(sum);
	evaluate(pos, sum);

	// outputting

	auto label{ "|            | w[MG]  w[EG] | b[MG]  b[EG] |  [MG]   [EG] |\n" };
	auto row  { "+------------+--------------+--------------+--------------+\n" };
	sync::cout << row << label;

	for (auto f{ 0U }; f < feature.size(); ++f)
	{
		sync::cout << "|" << feature[f].append(12 - feature[f].size(), ' ') << "|";
		for (int col{ WHITE }; col <= BLACK; ++col)
		{
			for (int stage{ MG }; stage <= EG; ++stage)
			{
				if (f == MATERIAL)
				{
					sync::cout << "  ---- ";
					continue;
				}

				// adjusting the material score

				auto diff{ 0 };
				if (f <= QUEENS)
				{
					diff = bit::popcnt(pos.pieces[f] & pos.side[col]) * value[stage][f];
					sum[MATERIAL][stage][col] += diff;
				}
				sum[f][stage][col] -= diff;
				output::align(sum[f][stage][col]);
			}
			sync::cout << "|";
		}
		output::align(sum[f][MG][WHITE] - sum[f][MG][BLACK]);
		output::align(sum[f][EG][WHITE] - sum[f][EG][BLACK]);
		sync::cout << "|\n";
	}

	// summing up & considering draws

	auto score{ interpolate(pos, sum) };
	auto scale{ scale_draw(pos, score) };

	sync::cout << row;
	sync::cout.precision(2);
	sync::cout << score / 100.0 << std::endl;

	if (obvious_draw(pos))
		sync::cout << engine::contempt[pos.xturn] / 100.0 << " (draw)" << std::endl;
	else if (scale != score)
		sync::cout << scale / 100.0 << " (draw scaling)" << std::endl;
}

void eval::pieces(const board &pos, int sum[][2][2], int col)
{
	auto xcol{ col ^ 1 };
	struct attack_threat
	{
		int cnt{ };
		int sum{ };
	} threat;

	pinning::pin_down(pos, col);

	auto king_zone{ attack::king_map[pos.king_sq[xcol]] | bit::shift(attack::king_map[pos.king_sq[xcol]], push[xcol]) };
	auto pawn_attack{  attack::by_pawns(pos, xcol) & ~pos.side[BOTH] };
	auto pawn_defense{ attack::by_pawns(pos, col) };

	// knights

	auto pieces{ pos.side[col] & pos.pieces[KNIGHTS] };
	while (pieces)
	{
		auto sq{ bit::scan(pieces) };
		auto sq64{ 1ULL << sq };

		sum[KNIGHTS][MG][col] += psqt[xcol][KNIGHTS][MG][sq];
		sum[KNIGHTS][EG][col] += psqt[xcol][KNIGHTS][EG][sq];

		// generating attacks

		auto targets{ attack::knight_map[sq] & pin.moves[sq] };
		all_attacks[col] |= targets;

		targets &= ~((pos.side[col] & pos.pieces[PAWNS]) | pawn_attack);
		king_attack::add(KNIGHTS, pos, targets, king_zone, threat.cnt, threat.sum);

		// rewarding outposts

		if (sq64 & is_outpost[col] & pawn_defense)
		{
			auto weight{ 1 };

			if (!(front_span[col][sq] & ~bit::file[square::file(sq)] & pos.pieces[PAWNS] & pos.side[xcol]))
				weight += 3;

			sum[KNIGHTS][MG][col] += knight_outpost[MG] * weight;
			sum[KNIGHTS][EG][col] += knight_outpost[EG] * weight;
		}

		// mobility

		auto cnt{ bit::popcnt(targets) };
		sum[MOBILITY][MG][col] += knight_mob[MG][cnt];
		sum[MOBILITY][EG][col] += knight_mob[EG][cnt];

		phase += phase_value[KNIGHTS];
		pieces &= pieces - 1;
	}

	// bishops

	pieces = pos.side[col] & pos.pieces[BISHOPS];
	while (pieces)
	{
		auto sq{ bit::scan(pieces) };

		sum[BISHOPS][MG][col] += psqt[xcol][BISHOPS][MG][sq];
		sum[BISHOPS][EG][col] += psqt[xcol][BISHOPS][EG][sq];

		// generating attacks

		auto targets{ attack::by_slider<BISHOP>(sq, pos.side[BOTH] & ~(pos.pieces[QUEENS] & pos.side[col])) & pin.moves[sq] };
		all_attacks[col] |= targets;

		targets &= ~(pos.side[col] & pos.pieces[PAWNS]);
		king_attack::add(BISHOPS, pos, targets, king_zone, threat.cnt, threat.sum);

		// bishop pair bonus

		if (pieces & (pieces - 1))
		{
			sum[BISHOPS][MG][col] += bishop_pair[MG];
			sum[BISHOPS][EG][col] += bishop_pair[EG];
		}

		// mobility

		auto cnt{ bit::popcnt(targets) };
		sum[MOBILITY][MG][col] += bishop_mob[MG][cnt];
		sum[MOBILITY][EG][col] += bishop_mob[EG][cnt];

		phase += phase_value[BISHOPS];
		pieces &= pieces - 1;
	}

	// rooks

	pieces = pos.side[col] & pos.pieces[ROOKS];
	while (pieces)
	{
		auto sq{ bit::scan(pieces) };

		sum[ROOKS][MG][col] += psqt[xcol][ROOKS][MG][sq];
		sum[ROOKS][EG][col] += psqt[xcol][ROOKS][EG][sq];

		// generating attacks

		auto targets{ attack::by_slider<ROOK>(sq, pos.side[BOTH] & ~((pos.pieces[QUEENS] | pos.pieces[ROOKS]) & pos.side[col])) & pin.moves[sq] };
		all_attacks[col] |= targets;

		targets &= ~(pos.side[col] & pos.pieces[PAWNS]);
		king_attack::add(ROOKS, pos, targets, king_zone, threat.cnt, threat.sum);

		// being on a open or semi-open file

		if (!(bit::file[square::file(sq)] & pos.pieces[PAWNS] & pos.side[col]))
		{
			auto weight{ 1 };

			if (!(bit::file[square::file(sq)] & pos.pieces[PAWNS]))
				weight += 1;
			if (bit::file[square::file(sq)] & pos.pieces[KINGS] & pos.side[xcol])
				weight += 1;

			sum[ROOKS][MG][col] += rook_open_file[MG] * weight;
			sum[ROOKS][EG][col] += rook_open_file[EG] * weight;

		}

		// being on the 7th rank

		if (relative::rank(square::rank(sq), col) == R7)
		{
			if (bit::rank[relative::rank(R7, col)] & pos.pieces[PAWNS] & pos.side[xcol]
				|| bit::rank[relative::rank(R8, col)] & pos.pieces[KINGS] & pos.side[xcol])
			{
				sum[ROOKS][MG][col] += major_on_7th[MG];
				sum[ROOKS][EG][col] += major_on_7th[EG];
			}
		}

		// mobility

		auto cnt{ bit::popcnt(targets) };
		sum[MOBILITY][MG][col] += rook_mob[MG][cnt];
		sum[MOBILITY][EG][col] += rook_mob[EG][cnt];

		phase += phase_value[ROOKS];
		pieces &= pieces - 1;
	}

	// queens

	pieces = pos.side[col] & pos.pieces[QUEENS];
	while (pieces)
	{
		auto sq{ bit::scan(pieces) };

		sum[QUEENS][MG][col] += psqt[xcol][QUEENS][MG][sq];
		sum[QUEENS][EG][col] += psqt[xcol][QUEENS][EG][sq];

		// generating attacks

		auto targets{ attack::by_slider<QUEEN>(sq, pos.side[BOTH]) & pin.moves[sq] };
		all_attacks[col] |= targets;

		targets &= ~(pos.side[col] & pos.pieces[PAWNS]);
		king_attack::add(QUEENS, pos, targets, king_zone, threat.cnt, threat.sum);

		// being on the 7th rank

		if (relative::rank(square::rank(sq), col) == R7)
		{
			if (bit::rank[relative::rank(R7, col)] & pos.pieces[PAWNS] & pos.side[xcol]
				|| bit::rank[relative::rank(R8, col)] & pos.pieces[KINGS] & pos.side[xcol])
			{
				sum[QUEENS][MG][col] += major_on_7th[MG];
				sum[QUEENS][EG][col] += major_on_7th[EG];
			}
		}

		// mobility

		auto cnt{ bit::popcnt(targets) };
		sum[MOBILITY][MG][col] += queen_mob[MG][cnt];
		sum[MOBILITY][EG][col] += queen_mob[EG][cnt];

		phase += phase_value[QUEENS];
		pieces &= pieces - 1;
	}

	// king

	{
		sum[KINGS][MG][col] += psqt[xcol][KINGS][MG][pos.king_sq[col]];
		sum[KINGS][EG][col] += psqt[xcol][KINGS][EG][pos.king_sq[col]];

		// generating attacks

		all_attacks[col] |= attack::king_map[pos.king_sq[col]];

		// king safety threats

		if (threat.cnt >= 2)
		{
			auto weight{ std::min(threat.sum, 63) };

			if (!(pos.pieces[QUEENS] & pos.side[col]))
				weight /= 2;
			if (!(pos.pieces[ROOKS] & pos.side[col]))
				weight -= weight / 5;

			sum[KINGS][MG][xcol] -= king_threat[weight];
			sum[KINGS][EG][xcol] -= king_threat[weight];
		}
	}
	pinning::unpin();
}

void eval::pawns(const board &pos, int sum[][2][2])
{
	if (!pos.pieces[PAWNS]) return;

	assert(pos.pawn_key != 0ULL);
	pawn::hash &entry{ pawnhash.table[pos.pawn_key & pawn::mask] };

	if (entry.key != pos.pawn_key)
	{
		entry.clear();
		pawn_base(pos, entry);
		entry.key = pos.pawn_key;
	}

	pawn_addendum(pos, sum, entry, WHITE);
	pawn_addendum(pos, sum, entry, BLACK);
}

void eval::pawn_base(const board &pos, pawn::hash &entry)
{
	// evaluating everything that can be stored in the pawn hash table

	for (int col{ WHITE }; col <= BLACK; ++col)
	{
		auto xcol{ col ^ 1 };
		auto pawns{ pos.pieces[PAWNS] & pos.side[col] };

		while (pawns)
		{
			auto sq{ static_cast<int>(bit::scan(pawns)) };
			auto file{ square::file(sq) };

			// PSQT

			entry.score[MG][col] += psqt[xcol][PAWNS][MG][sq];
			entry.score[EG][col] += psqt[xcol][PAWNS][EG][sq];

			// isolated pawn

			if (!(adjacent[file] & pos.pieces[PAWNS] & pos.side[col]))
			{
				entry.score[MG][col] -= isolated[MG];
				entry.score[EG][col] -= isolated[EG];
			}

			// passed pawn

			if (path::is_passed(pos, sq, col, xcol, file))
				entry.passed[col] |= 1ULL << sq;

			pawns &= pawns - 1;
		}
	}
}

void eval::pawn_addendum(const board &pos, int sum[][2][2], pawn::hash &entry, int col)
{
	// evaluating everything pawn related that is not stored in the pawn hash table

	sum[PAWNS][MG][col] += entry.score[MG][col];
	sum[PAWNS][EG][col] += entry.score[EG][col];

	assert(abs(sum[PAWNS][MG][col]) < MATE_SCORE);
	assert(abs(sum[PAWNS][EG][col]) < MATE_SCORE);

	// pawn formations

	auto xcol{ col ^ 1 };
	auto k_file{ square::file(pos.king_sq[col]) };

	uint64 pawns[]{ pos.pieces[PAWNS] & pos.side[WHITE], pos.pieces[PAWNS] & pos.side[BLACK] };
	auto base_chain{ king_chain[col][k_file] };

	while (base_chain)
	{
		auto sq{ bit::scan(base_chain) };
		auto file{ square::file(sq) };

		// pawn shield

		auto pawn{ attack::by_slider<ROOK>(sq, pawns[col]) & pawns[col] & bit::file[file] };
		assert(bit::popcnt(pawn) <= 1);

		auto score{ pawn ? shield_rank[col][square::rank(bit::scan(pawn))] : shield_rank[WHITE][R7] };
		sum[KINGS][MG][col] -= (file == k_file) ? score * 2 : score;

		// pawn storm

		pawn = attack::by_slider<ROOK>(sq, pawns[xcol]) & pawns[xcol] & bit::file[file];
		assert(bit::popcnt(pawn) <= 1);

		score = { pawn ? storm_rank[xcol][square::rank(bit::scan(pawn))] : 0 };
		sum[PAWNS][MG][xcol] += (bit::shift(pawn, push[xcol]) & pawns[col]) ? score : 2 * score;

		base_chain &= base_chain - 1;
	}

	// passed pawn

	auto passed{ entry.passed[col] };
	while (passed)
	{
		auto sq{ static_cast<int>(bit::scan(passed)) };
		auto rank{ square::rank(sq) };
		auto file{ square::file(sq) };

		// assigning a big bonus for a normal passed pawn

		auto mg_bonus{ passed_rank[col][rank] };
		auto eg_bonus{ passed_rank[col][rank] };

		// adding a king distance bonus

		auto stop_sq{ sq + push[col * 2] };
		eg_bonus += king_pp_distance[0] * square::distance(stop_sq, pos.king_sq[xcol]);
		eg_bonus -= king_pp_distance[1] * square::distance(stop_sq, pos.king_sq[col]);

		// adding X-ray attacks by major pieces to the attack- and defense-table

		auto path{ bit::file[file] & front_span[col][sq] };
		auto major{ bit::file[file] & front_span[xcol][sq] & (pos.pieces[ROOKS] | pos.pieces[QUEENS]) };

		auto attacked{ all_attacks[xcol] };
		auto defended{ all_attacks[col] };

		if (major && (major & attack::by_slider<ROOK>(sq, pos.side[BOTH])))
		{
			if (major & pos.side[col])
				defended |= path;
			else
				attacked |= path;
		}

		// penalising if the path to promotion is blocked, attacked or undefended

		auto blocked_path{ path & (pos.side[xcol] | (attacked & ~defended)) };

		if (blocked_path)
		{
			auto sq_cnt{ bit::popcnt(blocked_path) };
			assert(sq_cnt <= 6);

			mg_bonus /= sq_cnt + 2;
			eg_bonus /= sq_cnt + 1;
		}

		sum[PASSED][MG][col] += mg_bonus;
		sum[PASSED][EG][col] += eg_bonus;

		passed &= passed - 1;
	}
}