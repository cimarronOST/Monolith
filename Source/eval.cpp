/*
  Monolith Copyright (C) 2017-2026 Jonas Mayr

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


#include <bit>
#include <algorithm>
#include <cmath>
#include <array>

#include "main.h"
#include "types.h"
#include "attack.h"
#include "bit.h"
#include "board.h"
#include "eval.h"

namespace
{
	constexpr std::array<int, 6> phase_value{ { 0, 2, 2, 3, 9, 0 } };

	int sum_material(const board& pos, color cl)
	{
		// summing up phase values of all pieces on the board

		int sum{};
		for (piece pc : { KNIGHT, BISHOP, ROOK, QUEEN })
			sum += std::popcount(pos.pieces[pc] & pos.side[cl]) * phase_value[pc];
		return sum;
	}

	int interpolate(int sc, int phase)
	{
		// interpolating the mid- & end-game scores

		static const int max_weight
		{
             16 * phase_value[PAWN]
            + 4 * phase_value[KNIGHT]
            + 4 * phase_value[BISHOP]
            + 4 * phase_value[ROOK]
            + 2 * phase_value[QUEEN]
		};

		verify(phase >= 0);
		int weight{ std::min(phase, max_weight) };
		return (S_MG(sc) * weight + S_EG(sc) * (max_weight - weight)) / max_weight;
	}

	// assessing material constellations

	bool opposite_bishops(const board& pos)
	{
        return std::popcount(pos.pieces[BISHOP] & pos.side[WHITE]) == 1
            && std::popcount(pos.pieces[BISHOP] & pos.side[BLACK]) == 1
            && (pos.pieces[BISHOP] & bit::sq_white)
            && (pos.pieces[BISHOP] & bit::sq_black);
	}

	bool pawn_passed(const board& pos, square sq, bit64& sq_stop, color cl, color cl_x)
	{
		verify(cl == (cl_x ^ 1));

		return !(bit::fl_in_front[cl][sq] & pos.pieces[PAWN])
			&& !(bit::front_span[cl][bit::scan(sq_stop)] & pos.pieces[PAWN] & pos.side[cl_x]);
	}

	bool obvious_draw(const board& pos)
	{
		// recognizing positions with insufficient mating material
		// KvK, KNvK, KBvK, KB*vKB*, KNNvK

        if (pos.lone_bishops() && std::popcount(pos.side[BOTH]) <= 4
            && (!(bit::sq_white & pos.pieces[BISHOP]) || !(bit::sq_black & pos.pieces[BISHOP])))
			return true;

        if (pos.lone_knights() && std::popcount(pos.side[BOTH]) <= 4
            && (!(pos.pieces[KNIGHT] & pos.side[WHITE]) || !(pos.pieces[KNIGHT] & pos.side[BLACK])))
			return true;

		return false;
	}

	// managing attacks

	struct king_pressure
	{
		// keeping track of attacks near the enemy king which increase the pressure on the king
		// the collected scores are used for king safety evaluation

		bit64 king_zone;
		int cnt{}, sum{};

		king_pressure(const board& pos, color cl) : king_zone(bit::king_zone[cl][pos.sq_king[cl]]) {}
		void add(piece pc, bit64& targets)
		{
			// adding attacks

            if (pc == KING) return;
			if (bit64 king_attack{ targets & king_zone }; king_attack)
			{
				cnt += 1;
				sum += eval::threat_king_weight[pc] * std::popcount(king_attack);
			}
		}
	};

	struct attacks
	{
		// bundling all attacks used in the evaluation

		king_pressure pressure;
		std::array<bit64, 6> pc;
		bit64 by_1;
		bit64 by_2;
	};
}

namespace eval
{
	static void passed_pawns(const board& pos, std::array<int, 2>& sum, const std::array<attacks, 2>& att, kingpawn_hash::hash& entry, color cl)
	{
		// evaluating passed pawns (~200 Elo)
		// this is the only pawn-related evaluation that cannot be stored in the king-pawn hash table

		color  cl_x{ cl ^ 1 };
		bit64  passed{ entry.passed[cl] };
		while (passed)
		{
			square sq{ bit::scan(passed) };
			rank   rk{ type::rk_of(sq) };
			int bonus{ S(S_MG(passed_rank[cl][rk]), S_MG(passed_rank[cl][rk])) };

			// adding a king distance bonus (~20 Elo)

			square sq_stop{ sq + shift::push1x[0] * (cl == WHITE ? 1 : -1) };
			int dist_cl   { type::sq_distance(sq_stop, pos.sq_king[cl]) };
			int dist_cl_x { type::sq_distance(sq_stop, pos.sq_king[cl_x]) };
			bonus += king_dist_passed_cl * dist_cl + king_dist_passed_cl_x * dist_cl_x;

			// adding X-ray attacks by major pieces behind the passed pawn to the attack- and defense-table (~15 Elo)

			bit64 attacked{ att[cl_x].by_1 };
			bit64 defended{ att[cl].by_1 };

			bit64 major{ bit::fl_in_front[cl_x][sq] & (pos.pieces[ROOK] | pos.pieces[QUEEN]) };
			if (major && (major & attack::by_slider<ROOK>(sq, pos.side[BOTH])))
			{
				if (major & pos.side[cl])
					defended |= bit::fl_in_front[cl][sq];
				else
					attacked |= bit::fl_in_front[cl][sq];
			}

			// penalizing if the path to promotion is blocked, attacked or undefended (~80 Elo)

			if (bit64 blocked_path{ bit::fl_in_front[cl][sq] & (pos.side[cl_x] | (attacked & ~defended)) }; blocked_path)
			{
				int blocked{ std::popcount(blocked_path) };
				verify(blocked <= 6);
				bonus = S(S_MG(bonus) / (blocked + 2), S_EG(bonus) / (blocked + 1));
			}

			sum[cl] += bonus;
			passed  &= passed - 1;
		}
	}

	static void pawn_shield(int& score, const bit64& pawns, color cl, file fl_king)
	{
		// pawn shield evaluation (~35 Elo)

		for (file fl : { file(fl_king + 1), fl_king, file(fl_king - 1) })
		{
			if (!type::fl(fl)) continue;
			square sq{ type::sq_of(type::rk_of(RANK_1, cl), fl) };
			bit64 sq_pawn{ attack::by_slider<ROOK>(sq, pawns) & bit::file[fl] & pawns };
			verify(std::popcount(sq_pawn) <= 1);

			int sc{ sq_pawn ? shield_rank[cl][type::rk_of(bit::scan(sq_pawn))]
							: shield_rank[cl][type::rk_of(RANK_8, cl)] };
			score += (fl == fl_king ? sc * 2 : sc);
		}
	}

	static void pawns(const board& pos, kingpawn_hash::hash& entry)
	{
		// evaluating everything pawn-related that can be stored in the king-pawn hash table

		for (color cl : {WHITE, BLACK})
		{
			color cl_x{ cl ^ 1 };
			file  fl_king{ type::fl_of(pos.sq_king[cl]) };
			entry.attack[cl_x] = attack::by_pawns(pos.pieces[PAWN] & pos.side[cl_x], cl_x);
			bit64 pawns{ pos.pieces[PAWN] & pos.side[cl] };
			bit64 pawns_cl{ pawns };

			// pawn shield evaluation (~35 Elo)

			pawn_shield(entry.score[cl], pawns, cl, fl_king);

			// penalty for the king on a flank without pawns (~20 Elo)

			if (!(pos.pieces[PAWN] & bit::flank[fl_king]))
				entry.score[cl] += king_without_pawns;

			while (pawns)
			{
				square sq{ bit::scan(pawns) };
				bit64 sq_bit{ bit::set(sq) };
				bit64 sq_stop{ bit::shift(sq_bit, shift::push1x[cl]) };
				file fl{ type::fl_of(sq) };
				rank rk{ type::rk_of(sq) };

				// evaluating pawn position & pawn value & finding passed pawns

				entry.score[cl] += pawn_psq[cl][sq] + piece_value[PAWN];
				if (pawn_passed(pos, sq, sq_stop, cl, cl_x))
					entry.passed[cl] |= sq_bit;

				// penalizing isolated pawn (~10 Elo) & backward pawn (~0 Elo)

				if (!(bit::fl_adjacent[fl] & pawns_cl))
					entry.score[cl] += isolated;

				if ((entry.attack[cl_x] & sq_stop) && !((bit::front_span[cl_x][bit::scan(sq_stop)] & pawns_cl) ^ sq_bit))
					entry.score[cl] += backward;

				// rewarding connected pawn (~30 Elo)

				else if (bit::connected[cl][sq] & pawns_cl)
					entry.score[cl] += connect_rank[cl][rk];

				pawns &= pawns - 1;
			}
		}
	}

	static void tactics(const board& pos, std::array<int, 2>& sum, const std::array<attacks, 2>& att, color cl)
	{
		// penalizing tactical threats

		color cl_x{ cl ^ 1 };
		bit64 minors{ pos.pieces[KNIGHT] | pos.pieces[BISHOP] };
		bit64 minor_attacks{ att[cl_x].pc[KNIGHT] | att[cl_x].pc[BISHOP] };
		int threat{};

		// penalizing threats against unsupported pawns (~5 Elo)

		threat = std::popcount(pos.pieces[PAWN] & pos.side[cl] & (att[cl_x].by_1 & ~att[cl].by_1));
		sum[cl] += threat * threat_pawn;

		// penalizing threats against minor pieces (~15 Elo)

		threat = std::popcount(minors & pos.side[cl] & minor_attacks);
		sum[cl] += threat * threat_minor;

		// penalizing threats against rooks (~0 Elo)

		threat = std::popcount(pos.pieces[ROOK] & pos.side[cl] & minor_attacks);
		sum[cl] += threat * threat_rook;

		// penalizing threats against all pieces from pawns (~15 Elo)

		threat = std::popcount((minors | pos.pieces[ROOK] | pos.pieces[QUEEN]) & pos.side[cl] & att[cl_x].pc[PAWN]);
		sum[cl] += threat * threat_piece_by_pawn;

		// penalizing threats against all pieces from kings (~10 Elo)

		threat = std::popcount(att[cl_x].pc[KING] & (pos.side[cl] & ~(att[cl].pc[PAWN] | (att[cl].by_2 & ~att[cl_x].by_2))));
		sum[cl] += threat * threat_piece_by_king;

		// penalizing threats against queens from minor pieces (~10 Elo)

		threat = std::popcount(pos.pieces[QUEEN] & pos.side[cl] & minor_attacks);
		sum[cl] += threat * threat_queen_by_minor;

		// penalizing threats against queens from rooks (~0 Elo)

		threat = std::popcount(pos.pieces[QUEEN] & pos.side[cl] & att[cl_x].pc[ROOK]);
		sum[cl] += threat * threat_queen_by_rook;
	}

	static void king_safety(const board& pos, std::array<int, 2>& sum, const std::array<attacks, 2>& att, color cl)
	{
		// evaluating king safety threats

		color cl_x{ cl ^ 1 };
		bit64 weak_sq{ att[cl].by_1 & (~att[cl_x].by_1 | att[cl_x].pc[QUEEN] | att[cl_x].pc[KING]) & ~att[cl_x].by_2 };
		bit64 safe_sq{ (~att[cl_x].by_1 | (weak_sq & att[cl].by_2)) & ~pos.side[cl] };

		bit64 knight_reach{ bit::pc_attack[KNIGHT][pos.sq_king[cl_x]] };
		bit64 bishop_reach{ attack::by_slider<BISHOP>(pos.sq_king[cl_x], pos.side[BOTH]) };
		bit64   rook_reach{ attack::by_slider<ROOK>  (pos.sq_king[cl_x], pos.side[BOTH]) };

		// if the enemy king is attacked by at least 2 pieces, it is considered as threated

		if (att[cl_x].pressure.cnt >= 2)
		{
			// the king threat score is consolidated by all attacks of the king zone (~35 Elo)
			// if no queen is involved in the attack, the score is halved

			int weight{ att[cl_x].pressure.sum };
			weight /= (pos.pieces[QUEEN] & pos.side[cl]) ? 1 : 2;
			sum[cl] += threat_king_sum[std::min(weight, 59)];

			// weak squares around the king are an addition to the threat (~15 Elo)

			sum[cl] += weak_king_sq * std::popcount(att[cl_x].pressure.king_zone & weak_sq) ;
		}

		// the enemy king is also considered threatened if a piece can give a safe check on the next move (~25 Elo)
		// safe check threat of knights

		int cnt{ std::popcount(knight_reach & att[cl].pc[KNIGHT] & safe_sq) };
		sum[cl] += threat_king_by_check[KNIGHT] * cnt;

		// safe check threat of bishops

		cnt = std::popcount(bishop_reach & att[cl].pc[BISHOP] & safe_sq);
		sum[cl] += threat_king_by_check[BISHOP] * cnt;

		// safe check threat of rooks

		cnt = std::popcount(rook_reach & att[cl].pc[ROOK] & safe_sq);
		sum[cl] += threat_king_by_check[ROOK] * cnt;

		// safe check threat of queens

		cnt = std::popcount((bishop_reach | rook_reach) & att[cl].pc[QUEEN] & safe_sq);
		sum[cl] += threat_king_by_check[QUEEN] * cnt;

		// x-ray attacks can also threaten the king or pin pieces (~10 Elo)

		bit64 x_ray_pc{ (bit::pc_attack[BISHOP][pos.sq_king[cl_x]]
						& ((pos.pieces[BISHOP] | pos.pieces[QUEEN]) & pos.side[cl]))
			         | (bit::pc_attack[ROOK][pos.sq_king[cl_x]]
						& ((pos.pieces[ROOK]   | pos.pieces[QUEEN]) & pos.side[cl])) };

		while (x_ray_pc)
		{
			square sq{ bit::scan(x_ray_pc) };
			if (!(bit::ray[pos.sq_king[cl_x]][sq] & pos.pieces[PAWN]))
				sum[cl] += threat_king_by_xray[pos.piece_on[sq]];
			x_ray_pc &= x_ray_pc - 1;
		}
	}

	static int initiative(const board &pos, int sc, const kingpawn_hash::hash& entry)
	{
		// evaluating the initiative of the side that has the advantage (~5 Elo)
		// the computed score is applied as a correction
		// idea from Stockfish

		int outflanking{  std::abs(type::fl_of(pos.sq_king[WHITE]) - type::fl_of(pos.sq_king[BLACK]))
					    - std::abs(type::rk_of(pos.sq_king[WHITE]) - type::rk_of(pos.sq_king[BLACK])) };
		int passed_pawns{ std::popcount(entry.passed[WHITE] | entry.passed[BLACK]) };
		int sc_eg{ S_EG(sc) };

		bool pawns_on_both_flanks{ (pos.pieces[PAWN] & bit::half_east) && (pos.pieces[PAWN] & bit::half_west) };
		bool almost_unwinnable{ outflanking < 0 && !passed_pawns && !pawns_on_both_flanks };

		int sc_complexity =
			+ complexity[0] * std::popcount(pos.pieces[PAWN])
			+ complexity[1] * outflanking
			+ complexity[2] * pawns_on_both_flanks
			+ complexity[3] * pos.lone_pawns()
			+ complexity[4] * almost_unwinnable
			+ complexity[5];

		// if the score is positive, white has the advantage, otherwise black
		// the score is also not allowed to change sign after the correction

		return ((sc_eg > 0) - (sc_eg < 0)) * std::max(S_EG(sc_complexity), -std::abs(sc_eg));
	}

	static void get_att(piece pc, bit64& pieces, color cl, const board& pos, square& sq, bit64& tar,
		std::array<attacks, 2>& att, const bit64& mob_area, int& phase)
	{
		// calculating piece attacks
		
		sq = bit::scan(pieces);
		switch (pc)
		{
		case PAWN:   verify(false); break;
		case KNIGHT: tar = bit::pc_attack[pc][sq] & mob_area; break;
		case BISHOP: tar = attack::by_slider<BISHOP>(sq, pos.side[BOTH] ^ pos.pieces[QUEEN]) & mob_area; break;
		case ROOK:   tar = attack::by_slider<ROOK  >(sq, pos.side[BOTH] & ~(pos.pieces[QUEEN]
							| (pos.pieces[ROOK] & pos.side[cl]))) & mob_area; break;
		case QUEEN:  tar = attack::by_slider<QUEEN >(sq, pos.side[BOTH]) & mob_area; break;
		case KING:   tar = bit::pc_attack[pc][sq]; break;
		default: verify(type::pc(pc));
		}

		att[cl].pc[pc] |= tar;
		att[cl].by_2   |= tar & att[cl].by_1;
		att[cl].by_1   |= tar;
		phase += phase_value[pc];
		att[cl^1].pressure.add(pc, tar);
	}

	static void pieces(const board& pos, std::array<int, 2>& sum, std::array<attacks, 2>& att, int& phase, color cl)
	{
		// evaluating all pieces except pawns
		// starting by initializing king pressure & finding all pins to restrict piece mobility

		color cl_x{ cl ^ 1 };
        bit64 pawns_cl  { pos.pieces[PAWN] & pos.side[cl] };
        bit64 pawns_cl_x{ pos.pieces[PAWN] & pos.side[cl_x] };

		// defining the mobility area

		bit64 blocked_pawns{ pawns_cl & bit::shift(pos.side[BOTH], shift::push1x[cl_x]) };
		bit64 mob_area{ ~(blocked_pawns | ((pos.pieces[KING] | pos.pieces[QUEEN]) & pos.side[cl]) | att[cl_x].pc[PAWN])};
		bit64 targets{}, pieces; square sq{};

		// starting with the evaluation of knights

        pieces = pos.pieces[KNIGHT] & pos.side[cl];
		while (pieces)
		{
			// evaluating piece position (~15 Elo), mobility (~60 Elo) & piece value

            get_att(KNIGHT, pieces, cl, pos, sq, targets, att, mob_area, phase);
			sum[cl] += knight_psq[cl][sq] +piece_value[KNIGHT];
            sum[cl] += knight_mobility[std::popcount(targets)];

			// rewarding outposts (~15 Elo)

            if (bit::set(sq) & bit::outpost_zone[cl] & att[cl].pc[PAWN])
			{
				int weight{ (bit::fork_in_front[cl][sq] & pawns_cl_x) ? 1 : 4 };
				sum[cl] += knight_outpost * weight;
			}

			// penalizing long distance to kings (~5 Elo)

			int king_distance{ std::min(type::sq_distance(sq, pos.sq_king[cl]), type::sq_distance(sq, pos.sq_king[cl_x])) };
			if (king_distance > 3)
				sum[cl] += knight_distance_kings[king_distance - 4];

			pieces &= pieces - 1;
		}

		// bishop

        pieces = pos.pieces[BISHOP] & pos.side[cl];
		while (pieces)
		{
			// evaluating piece position (~25 Elo), mobility (~100 Elo) & piece value

            get_att(BISHOP, pieces, cl, pos, sq, targets, att, mob_area, phase);
            sum[cl] += bishop_psq[cl][sq] + piece_value[BISHOP];
            sum[cl] += bishop_mobility[std::popcount(targets)];

			// bishop pair bonus (~40 Elo)

			if (pieces & (pieces - 1))
				sum[cl] += bishop_pair;

			// penalty for pawns on same colored squares (~15 Elo)

            if (bit::set(sq) & bit::sq_white)
                sum[cl] += bishop_color_pawns * std::popcount(pawns_cl & bit::sq_white);
            else
                sum[cl] += bishop_color_pawns * std::popcount(pawns_cl & bit::sq_black);

			// penalty if the enemy half of the board cannot be reached (~15 Elo)

			if (!(targets & bit::board_half[cl_x]))
				sum[cl] += bishop_trapped;

			pieces &= pieces - 1;
		}

		// rook

        pieces = pos.pieces[ROOK] & pos.side[cl];
		while (pieces)
		{
			// evaluating piece position (~10 Elo), mobility (~40 Elo) & piece value

            get_att(ROOK, pieces, cl, pos, sq, targets, att, mob_area, phase);
            sum[cl] += rook_psq[cl][sq] + piece_value[ROOK];
            sum[cl] += rook_mobility[std::popcount(targets)];

			// being on a open or semi-open file (~20 Elo)

            bit64 fl_rook{ bit::file[type::fl_of(sq)] };
            if (!(fl_rook & pawns_cl))
			{
				int weight{ 1 + !(fl_rook & pos.pieces[PAWN])
                    + bool(fl_rook & pos.pieces[KING] & pos.side[cl_x]) };
				sum[cl] += rook_open_file * weight;
			}

			// being on the 7th rank (~0 Elo)

            if (type::rk_of(type::rk_of(sq), cl) == RANK_7)
                if (bit::rank[type::rk_of(RANK_7, cl)] & pawns_cl_x
                    || bit::rank[type::rk_of(RANK_8, cl)] & pos.pieces[KING] & pos.side[cl_x])
					sum[cl] += rook_on_7th;

			pieces &= pieces - 1;
		}

		// queen

        pieces = pos.pieces[QUEEN] & pos.side[cl];
		while (pieces)
		{
			// evaluating piece position (~10 Elo), mobility (~20 Elo) & piece value

            get_att(QUEEN, pieces, cl, pos, sq, targets, att, mob_area, phase);
            sum[cl] += queen_psq[cl][sq] + piece_value[QUEEN];
            sum[cl] += queen_mobility[std::popcount(targets)];

			pieces &= pieces - 1;
		}

		// king

        pieces = pos.pieces[KING] & pos.side[cl];
		{
			// evaluating piece position (~60 Elo)

            get_att(KING, pieces, cl, pos, sq, targets, att, mob_area, phase);
            sum[cl] += king_psq[cl][sq] + piece_value[KING];
		}
	}

	static int scale_towards_draw(const board& pos, int sc)
	{
		// scaling the score towards a draw if the chances for a win are slight

		verify(!obvious_draw(pos));
		color winning{ sc < 0 };
		color loosing{ winning ^ 1 };

		// down-scaling if there aren't many pawns left (~15 Elo)

		if (sum_material(pos, winning) - sum_material(pos, loosing) <= phase_value[KNIGHT])
			if (int pawns{ std::popcount(pos.side[winning] & pos.pieces[PAWN]) }; pawns <= 2)
				sc = sc * eval::scale_few_pawns[pawns] / 16;

		// down-scaling for opposite colored bishops (~15 Elo)

		if ((pos.pieces[BISHOP] | pos.pieces[PAWN] | pos.pieces[KING]) == pos.side[BOTH] && opposite_bishops(pos))
			sc /= 2;

		return sc;
	}

	static void evaluate(const board& pos, std::array<int, 2>& sum, int& phase, kingpawn_hash::hash& entry)
	{
		// beginning with the evaluation of the position

		if (pos.pieces[PAWN])
		{
			// creating a new table entry if nothing is found in the pawn hash table
			// the hash table speeds up the engine considerably (~15 Elo)
			// a table entry provides some basic pawn evaluation

			verify(pos.key.pawn);
			if (key64 key{ zobrist::kingpawn_key(pos) }; key != entry.key)
			{
				entry = kingpawn_hash::hash{};
				pawns(pos, entry);
				entry.key = key;
			}

			sum[WHITE] = entry.score[WHITE];
			sum[BLACK] = entry.score[BLACK];
		}

		// initializing attack tables

		std::array<attacks, 2> att{{ { { pos, WHITE }, { entry.attack[WHITE] }, entry.attack[WHITE], {} },
									 { { pos, BLACK }, { entry.attack[BLACK] }, entry.attack[BLACK], {} } }};

		// evaluating pieces

		pieces(pos, sum, att, phase, WHITE);
		pieces(pos, sum, att, phase, BLACK);

		// evaluating tactical threats against pieces & pawns

		tactics(pos, sum, att, WHITE);
		tactics(pos, sum, att, BLACK);

		// evaluating king safety threats

		king_safety(pos, sum, att, WHITE);
		king_safety(pos, sum, att, BLACK);

		// extending the pawn evaluation

		passed_pawns(pos, sum, att, entry, WHITE);
		passed_pawns(pos, sum, att, entry, BLACK);
	}
}

score eval::static_eval(const board& pos, kingpawn_hash& hash)
{
	// entry point of the evaluation chain
	// filtering out obviously drawn positions with insufficient mating material first

	if (obvious_draw(pos))
		return DRAW;

	// initializing & probing the pawn hash table

	int phase{};
	std::array<int, 2> sum{};
	auto& entry{ hash.get_entry(pos) };

	// evaluating the position

	evaluate(pos, sum, phase, entry);

	// adding initiative correction (~5 Elo) before interpolating the scores

	int sc{ sum[WHITE] - sum[BLACK] };
	sc += initiative(pos, sc, entry);
	sc  = interpolate(sc, phase);
	verify(std::abs(sc) < int(LONGEST_MATE));

	// scaling drawn positions (~30 Elo)
	// also adjusting the sign relative to the side to move and adding a tempo bonus (~5 Elo)

	sc = scale_towards_draw(pos, sc);
	return score(sc * (pos.cl == WHITE ? 1 : -1) + tempo);
}

void eval::mirror_tables()
{
	// mirroring various tables for black

	for (square sq{ H1 }; sq <= A8; sq += 1)
	{
		  pawn_psq[WHITE][A8 - sq] =   pawn_psq[BLACK][sq];
		knight_psq[WHITE][A8 - sq] = knight_psq[BLACK][sq];
		bishop_psq[WHITE][A8 - sq] = bishop_psq[BLACK][sq];
		  rook_psq[WHITE][A8 - sq] =   rook_psq[BLACK][sq];
		 queen_psq[WHITE][A8 - sq] =  queen_psq[BLACK][sq];
		  king_psq[WHITE][A8 - sq] =   king_psq[BLACK][sq];
	}

	for (int rk{ RANK_1 }; rk <= RANK_8; ++rk)
	{
		connect_rank[BLACK][rk] = connect_rank[WHITE][RANK_8 - rk];
		 passed_rank[BLACK][rk] =  passed_rank[WHITE][RANK_8 - rk];
		 shield_rank[BLACK][rk] =  shield_rank[WHITE][RANK_8 - rk];
	}
}
