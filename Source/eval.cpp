/*
  Monolith 1.0  Copyright (C) 2017-2018 Jonas Mayr

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

#include "utilities.h"
#include "stream.h"
#include "uci.h"
#include "attack.h"
#include "bit.h"
#include "eval.h"

// material weights

int eval::piece_value[2][6]
{
	{  81, 354, 363, 522, 1080, 0 },
	{ 100, 323, 340, 581, 1123, 0 }
};

int eval::phase_value[6]{ 0, 2, 2, 3, 9 };

// piece weights

int eval::bishop_pair[2]   { 33, 55 };
int eval::knight_outpost[2]{  6,  5 };

int eval::major_on_7th[2]  { -5, 25 };
int eval::rook_open_file[2]{ 16,  4 };

// threat weights

int eval::pawn_threat[2]       {  10, -12 };
int eval::minor_threat[2]      { -38, -20 };
int eval::queen_threat[2]      { -50,  28 };
int eval::queen_threat_minor[2]{  23,   3 };

// king threat weights
// credits go to https://www.chessprogramming.org/King_Safety

int eval::king_threat_weight[5]{ 0, 2, 2, 3, 5 };
int eval::king_threat[64]
{
	  0,   0,   0,   2,   3,   5,   7,   9,
	 12,  15,  18,  22,  26,  30,  35,  39,
	 44,  50,  56,  62,  68,  75,  82,  85,
	 89,  97, 105, 113, 122, 131, 140, 150,
	169, 180, 191, 202, 213, 225, 237, 248,
	260, 272, 283, 295, 307, 319, 330, 342,
	354, 366, 377, 389, 401, 412, 424, 436,
	448, 459, 471, 483, 494, 500, 500, 500
};

// pawn weights

int eval::isolated[2]{ -6, -12 };
int eval::backward[2]{ -3,  -4 };

int eval::connected[2][2][8]
{ {
	{  0,  4, 15, 16, 22, 35, 89,  0 },
	{  0, -5,  3,  3,  9, 37, 22,  0 }
} };

int eval::king_distance_enemy[2] {  0, 22 };
int eval::king_distance_friend[2]{ -4, -6 };

int eval::passed_rank[2][8]{ {  0, -27, -26,   6,  52, 149, 289,   0 } };
int eval::shield_rank[2][8]{ {  0,  -1,  -7, -13, -16,  -9, -34, -30 } };
int eval::storm_rank[ 2][8]{ {  0,   0,   0,  -1,   5,   9,   0,   0 } };

// mobility weights

int eval::knight_mobility[2][9]
{
	{ -10, -13,  2,  6, 18, 24, 31, 38, 44, },
	{ -37, -27, -3, 12, 12, 19, 20, 21, 15, }
};
int eval::bishop_mobility[2][14]
{
	{ -18,  -4,  11, 13, 21, 28, 34, 37, 38, 40, 41, 44, 42, 56, },
	{ -22, -26, -22, -3, 10, 19, 21, 27, 33, 32, 31, 33, 29, 30, }
};
int eval::rook_mobility[2][15]
{
	{ -21, -18,  -9, -2,  4,  4,  3,  3,  7, 12, 11, 11, 20, 23, 21, },
	{ -93, -42, -20, -1,  8, 18, 27, 29, 32, 35, 42, 45, 46, 55, 56, }
};
int eval::queen_mobility[2][28]
{
	{ -30, -20, -20, -14, -12,  -5,  -5,   4,   7,  9, 11, 13, 14, 14, 14, 15, 20, 17, 18, 22, 22, 30, 39, 43, 55, 59, 66, 80, },
	{ -30, -13, -21, -15, -42, -57, -35, -29, -30, -7, -3,  4,  8, 22, 33, 37, 39, 47, 52, 54, 65, 61, 53, 54, 48, 55, 50, 60, }
};

// piece square tables

int eval::pawn_psq[2][2][64]
{ {
	{
		  0,   0,   0,   0,   0,   0,   0,   0,
		-10,   8,  14,  20,  20,  14,   8, -10,
		 17,  31,  41,  35,  35,  41,  31,  17,
		  0,   4,   7,  15,  15,   7,   4,   0,
		-11,  -8,   1,  11,  11,   1,  -8, -11,
		-10,  -7,  -5,  -4,  -4,  -5,  -7, -10,
		-11,   8,   5,  -6,  -6,   5,   8, -11,
		  0,   0,   0,   0,   0,   0,   0,   0,
	},
	{
		  0,   0,   0,   0,   0,   0,   0,   0,
		 37,  31,  15,   8,   8,  15,  31,  37,
		  9,   5,  -7, -21, -21,  -7,   5,   9,
		  2,  -2,  -7, -11, -11,  -7,  -2,   2,
		 -8,  -9,  -8, -10, -10,  -8,  -9,  -8,
		-16, -14,  -8,  -1,  -1,  -8, -14, -16,
		-12, -10,   1,   5,   5,   1, -10, -12,
		  0,   0,   0,   0,   0,   0,   0,   0,
	}
} };
int eval::knight_psq[2][2][64]
{ {
	{
	   -113, -64, -73, -20, -20, -73, -64,-113,
		-16,  -4,  46,  16,  16,  46,  -4, -16,
		 10,  41,  35,  49,  49,  35,  41,  10,
		 22,  22,  26,  32,  32,  26,  22,  22,
		  8,  22,  19,  23,  23,  19,  22,   8,
		 -9,   3,  12,  21,  21,  12,   3,  -9,
		-10,  -3,   8,  10,  10,   8,  -3, -10,
		-40,  -1,  -7,   6,   6,  -7,  -1, -40,
	},
	{
		-58, -36,  -4, -17, -17,  -4, -36, -58,
		-20,  -7, -16,   4,   4, -16,  -7, -20,
		-20,  -6,   9,  11,  11,   9,  -6, -20,
		 -3,  17,  21,  27,  27,  21,  17,  -3,
		 -3,   4,  18,  26,  26,  18,   4,  -3,
		-13,  -2,   3,  21,  21,   3,  -2, -13,
		-16,  -7,  -6,   4,   4,  -6,  -7, -16,
		 -9, -22,  -3,  -3,  -3,  -3, -22,  -9,
	}
} };
int eval::bishop_psq[2][2][64]
{ {
	{
		-53, -51, -57, -60, -60, -57, -51, -53,
		-62, -17,  -9, -35, -35,  -9, -17, -62,
		  4,   9,  29,  31,  31,  29,   9,   4,
		-16,  -5,   7,  21,  21,   7,  -5, -16,
		 -2,  -6,  -4,  16,  16,  -4,  -6,  -2,
		 -5,   9,  10,   1,   1,  10,   9,  -5,
		 -3,  20,   7,  -4,  -4,   7,  20,  -3,
		-19,  -6,  -4,  -9,  -9,  -4,  -6, -19,
	},
	{
		-14,  -7, -10,  -1,  -1, -10,  -7, -14,
		  2,  -5,  -5,  -4,  -4,  -5,  -5,   2,
		 -3,  -3,  -5, -12, -12,  -5,  -3,  -3,
		  1,   3,   4,   8,   8,   4,   3,   1,
		-12,  -2,   5,   6,   6,   5,  -2, -12,
		-12,  -4,  -1,   6,   6,  -1,  -4, -12,
		-17, -15,  -9,  -4,  -4,  -9, -15, -17,
		 -5,  -2,  -4,  -2,  -2,  -4,  -2,  -5,
	}
} };
int eval::rook_psq[2][2][64]
{ {
	{
		 12,   1, -12,  14,  14, -12,   1,  12,
		  5,  -6,  25,  18,  18,  25,  -6,   5,
		 -3,   7,  -9,   4,   4,  -9,   7,  -3,
		-11,  -9,  -4,  -1,  -1,  -4,  -9, -11,
		-25, -11, -14, -15, -15, -14, -11, -25,
		-21,  -8,  -9,  -5,  -5,  -9,  -8, -21,
		-27,  -6,   0,   1,   1,   0,  -6, -27,
		 -4,  -8,   1,   5,   5,   1,  -8,  -4,
	},
	{
		 12,  17,  24,  17,  17,  24,  17,  12,
		  1,  11,   4,   4,   4,   4,  11,   1,
		  7,  11,  15,  10,  10,  15,  11,   7,
		 10,   7,  15,   9,   9,  15,   7,  10,
		  6,   9,   8,   8,   8,   8,   9,   6,
		 -6,  -2,   0,  -4,  -4,   0,  -2,  -6,
		 -5, -11,  -6,  -7,  -7,  -6, -11,  -5,
		 -8,  -6,  -3,  -8,  -8,  -3,  -6,  -8,
	}
} };
int eval::queen_psq[2][2][64]
{ {
	{
		-11,  10,   2,   8,   8,   2,  10, -11,
		 26, -39,  17, -12, -12,  17, -39,  26,
		 19,  16,   5,  -3,  -3,   5,  16,  19,
		 -2,  -8,  -1,  -8,  -8,  -1,  -8,  -2,
		  0,  10,  -3,  -5,  -5,  -3,  10,   0,
		  8,  16,   8,   4,   4,   8,  16,   8,
		  5,  21,  22,  18,  18,  22,  21,   5,
		 16,   1,   0,  18,  18,   0,   1,  16,
	},
	{
		 22,  22,  36,  34,  34,  36,  22,  22,
		-15,  31,  15,  55,  55,  15,  31, -15,
		 15,   5,  40,  65,  65,  40,   5,  15,
		 38,  59,  41,  63,  63,  41,  59,  38,
		 26,  21,  38,  53,  53,  38,  21,  26,
		 -1,   3,  29,  23,  23,  29,   3,  -1,
		 -5, -24, -18,   2,   2, -18, -24,  -5,
		-29, -20,  -6, -19, -19,  -6, -20, -29,
	}
} };
int eval::king_psq[2][2][64]
{ {
	{
		  3,  21,  16,   8,   8,  16,  21,   3,
		 23,  41,  49,  50,  50,  49,  41,  23,
		 18,  80,  75,  39,  39,  75,  80,  18,
		-40,  31,  -9, -18, -18,  -9,  31, -40,
		-81,  -3, -24, -47, -47, -24,  -3, -81,
		-26,  16, -13, -25, -25, -13,  16, -26,
		 29,  52,  13, -18, -18,  13,  52,  29,
		 20,  52,  10,  26,  26,  10,  52,  20,
	},
	{
		-88, -31, -29, -37, -37, -29, -31, -88,
		-21,  -7,  -6,  -9,  -9,  -6,  -7, -21,
		-12,   7,   8,   0,   0,   8,   7, -12,
		-11,   3,  16,  15,  15,  16,   3, -11,
		-15,  -7,  11,  20,  20,  11,  -7, -15,
		-22, -12,   1,  11,  11,   1, -12, -22,
		-36, -28,  -9,   1,   1,  -9, -28, -36,
		-78, -53, -30, -35, -35, -30, -53, -78,
	}
} };

namespace
{
	// fast index tables filled at startup

	uint64 adjacent[8]{};
	uint64 front_span[2][64]{};
	uint64 king_chain[2][8]{};

	// some constants

	constexpr uint64 outpost_zone[]{ 0x3c3c3c000000, 0x3c3c3c0000 };

	const int max_weight
	{
		16 * eval::phase_value[PAWNS] +
		 4 * eval::phase_value[KNIGHTS] +
		 4 * eval::phase_value[BISHOPS] +
		 4 * eval::phase_value[ROOKS] +
		 2 * eval::phase_value[QUEENS]
	};

	const std::array<std::string, 10> feature
	{ { "pawns", "knights", "bishops", "rooks", "queens", "kings", "material", "mobility", "passed pawns", "threats" } };

	static_assert(WHITE == 0 && BLACK == 1, "index");
	static_assert(MG    == 0 && EG    == 1, "index");
}

namespace sum
{
	int scores(int sum[][2][2], game_stage stage)
	{
		// summing up scores of all evaluation features

		int score{};
		for (uint32 f{}; f < feature.size(); ++f)
			score += sum[f][stage][WHITE] - sum[f][stage][BLACK];
		return score;
	}

	int pieces(const board &pos, int color)
	{
		// summing up phase values of all pieces on the board

		int sum{};
		for (int p{ KNIGHTS }; p <= QUEENS; ++p)
			sum += bit::popcnt(pos.pieces[p] & pos.side[color]) * eval::phase_value[p];
		return sum;
	}

	void material(int sum[][2][2])
	{
		// initializing material sum to show correct output of the itemized evaluation

		for (int stage{ MG }; stage <= EG; ++stage)
		{
			for (auto &sum : sum[MATERIAL][stage])
			{
				sum = -8 * eval::piece_value[stage][PAWNS]
					 - 2 * eval::piece_value[stage][KNIGHTS]
					 - 2 * eval::piece_value[stage][BISHOPS]
					 - 2 * eval::piece_value[stage][ROOKS]
					 - 1 * eval::piece_value[stage][QUEENS];
			}
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
			&& (pos.pieces[BISHOPS] & bit::white)
			&& (pos.pieces[BISHOPS] & bit::black);
	}
}

namespace draw
{
	int scale(const board &pos, int score)
	{
		// scaling the score for drawish positions

		int winning{ score > 0 ? WHITE : BLACK };

		if (!(pos.side[winning] & pos.pieces[PAWNS])
			&& sum::pieces(pos, winning) - sum::pieces(pos, winning ^ 1) <= eval::phase_value[KNIGHTS])
			score /= 4;

		if (material::opposite_bishops(pos))
			score = score * 3 / 4;

		return score;
	}

	bool obvious(const board &pos)
	{
		// recognizing positions with insufficient mating material
		// KvK, KNvK, KBvK, KB*vKB*, KNNvK

		if (material::lone_bishops(pos) && (!(bit::white & pos.pieces[BISHOPS]) || !(bit::black & pos.pieces[BISHOPS])))
			return true;

		if (material::lone_knights(pos) && (!(pos.pieces[KNIGHTS] & pos.side[WHITE]) || !(pos.pieces[KNIGHTS] & pos.side[BLACK])))
			return true;

		return false;
	}
}

namespace score
{
	int interpolate(int sum[][2][2], int &phase)
	{
		// interpolating the mid- & end-game scores

		assert(phase >= 0);
		auto weight{ phase <= max_weight ? phase : max_weight };
		return (sum::scores(sum, MG) * weight + sum::scores(sum, EG) * (max_weight - weight)) / max_weight;
	}
}

namespace file
{
	bool open(const board &pos, int sq, int color, int file)
	{
		// checking whether a file is open or not

		return !(front_span[color][sq] & pos.pieces[PAWNS] & bit::file[file]);
	}
}

namespace path
{
	uint64 blocked(const board &pos, int sq, int color, int xcolor)
	{
		// checking whether the path in front is blocked, directly or indirectly

		return front_span[color][sq] & pos.pieces[PAWNS] & pos.side[xcolor];
	}

	bool is_passed(const board &pos, int sq, int color, int xcolor, int file)
	{
		// defining passed pawns
		
		assert(index::file(sq) == file);
		assert(pos.side[color] & (1ULL << sq));
		assert(color == (xcolor ^ 1));

		return file::open(pos, sq, color, file) && !blocked(pos, sq, color, xcolor);
	}
}

namespace output
{
	void align(double score)
	{
		// aligning all scores to create a itemized table after the "eval" command

		score /= 100.0;
		sync::cout.precision(2);
		sync::cout << std::fixed
			<< (score < 0.0 ? "" : " ")
			<< (std::abs(score) >= 10.0 ? "" : " ")
			<< score << " ";
	}
}

namespace king_threat
{
	void add(piece_index p, uint64 &targets, uint64 &king_zone, int &threat_count, int &threat_sum)
	{
		// adding attacks of the king for king safety evaluation

		assert(p <= QUEENS);
		auto king_zone_attack{ targets & king_zone };
		if  (king_zone_attack)
		{
			threat_count += 1;
			threat_sum   += eval::king_threat_weight[p] * bit::popcnt(king_zone_attack);
		}
	}
}

namespace eval
{
	void threats(const board &pos, int sum[][2][2], uint64 attacks[][6], uint64 attacks_final[], int color)
	{
		// evaluating threats against pieces & pawns
		// inspirational credits go to the Ethereal-authors (https://github.com/AndyGrant/Ethereal)

		auto xcolor{ color ^ 1 };
		int  threat{};

		// penalizing threats against unsupported pawns

		threat = bit::popcnt(pos.pieces[PAWNS] & pos.side[color] & ~attacks_final[color] & attacks_final[xcolor]);
		sum[THREATS][MG][color] += threat * pawn_threat[MG];
		sum[THREATS][EG][color] += threat * pawn_threat[EG];

		// penalizing threats against minor pieces

		threat = bit::popcnt((pos.pieces[KNIGHTS] | pos.pieces[BISHOPS]) & pos.side[color] & attacks[xcolor][PAWNS]);
		sum[THREATS][MG][color] += threat * minor_threat[MG];
		sum[THREATS][EG][color] += threat * minor_threat[EG];

		// penalizing threats against the queen

		auto queens{ pos.pieces[QUEENS] & pos.side[color] };
		if (queens & attacks_final[xcolor])
		{
			sum[THREATS][MG][color] += queen_threat[MG];
			sum[THREATS][EG][color] += queen_threat[EG];

			if (queens & (attacks[xcolor][KNIGHTS] | attacks[xcolor][BISHOPS]))
			{
				sum[THREATS][MG][color] += queen_threat_minor[MG];
				sum[THREATS][EG][color] += queen_threat_minor[EG];
			}
		}
	}

	void pawn_extended(const board &pos, int sum[][2][2], uint64 attacks_final[], pawn::hash &entry, int color)
	{
		// evaluating everything pawn-related that cannot be stored in the pawn hash-table
		// adding the hash-table information first

		sum[PAWNS][MG][color] += entry.score[MG][color];
		sum[PAWNS][EG][color] += entry.score[EG][color];

		assert(std::abs(sum[PAWNS][MG][color]) < SCORE_LONGEST_MATE);
		assert(std::abs(sum[PAWNS][EG][color]) < SCORE_LONGEST_MATE);

		// pawn formations

		auto xcolor{ color ^ 1 };
		auto k_file{ index::file(pos.sq_king[color]) };

		auto pawns_friend{ pos.pieces[PAWNS] & pos.side[color] };
		auto pawns_enemy { pos.pieces[PAWNS] & pos.side[xcolor] };
		auto base_chain  { king_chain[color][k_file] };

		while (base_chain)
		{
			auto sq{ bit::scan(base_chain) };
			auto file{ index::file(sq) };
			assert(relative::rank(index::rank(sq), color) == R1);

			// pawn shield

			auto pawn{ attack::by_slider<ROOK>(sq, pawns_friend) & pawns_friend & bit::file[file] };
			assert(bit::popcnt(pawn) <= 1);

			auto score{ pawn ? shield_rank[color][index::rank(bit::scan(pawn))] : shield_rank[WHITE][R8] };
			sum[KINGS][MG][color] += (file == k_file) ? score * 2 : score;

			// pawn storm

			pawn = attack::by_slider<ROOK>(sq, pawns_enemy) & pawns_enemy & bit::file[file];
			assert(bit::popcnt(pawn) <= 1);

			score = { pawn ? storm_rank[xcolor][index::rank(bit::scan(pawn))] : 0 };
			sum[PAWNS][MG][xcolor] += (bit::shift(pawn, shift::push[xcolor]) & pawns_friend) ? score : score * 2;

			base_chain &= base_chain - 1;
		}

		// passed pawn

		auto passed{ entry.passed[color] };
		while (passed)
		{
			auto sq{ static_cast<int>(bit::scan(passed)) };
			auto rank{ index::rank(sq) };
			auto file{ index::file(sq) };

			// assigning a big bonus for a normal passed pawn

			auto mg_bonus{ passed_rank[color][rank] };
			auto eg_bonus{ passed_rank[color][rank] };

			// adding a king distance bonus

			auto stop_sq{ sq + shift::push[color * 2] };
			auto distance_friend{ square::distance(stop_sq, pos.sq_king[color]) };
			auto distance_enemy { square::distance(stop_sq, pos.sq_king[xcolor]) };

			mg_bonus += king_distance_friend[MG] * distance_friend + king_distance_enemy[MG] * distance_enemy;
			eg_bonus += king_distance_friend[EG] * distance_friend + king_distance_enemy[EG] * distance_enemy;

			// adding X-ray attacks by major pieces to the attack- and defense-table

			auto path { bit::file[file] & front_span[color][sq] };
			auto major{ bit::file[file] & front_span[xcolor][sq] & (pos.pieces[ROOKS] | pos.pieces[QUEENS]) };

			auto attacked{ attacks_final[xcolor] };
			auto defended{ attacks_final[color] };

			if (major && (major & attack::by_slider<ROOK>(sq, pos.side[BOTH])))
			{
				if (major & pos.side[color])
					defended |= path;
				else
					attacked |= path;
			}

			// penalizing if the path to promotion is blocked, attacked or undefended

			auto blocked_path{ path & (pos.side[xcolor] | (attacked & ~defended)) };

			if (blocked_path)
			{
				auto sq_cnt{ bit::popcnt(blocked_path) };
				assert(sq_cnt <= 6);

				mg_bonus /= sq_cnt + 2;
				eg_bonus /= sq_cnt + 1;
			}

			sum[PASSED][MG][color] += mg_bonus;
			sum[PASSED][EG][color] += eg_bonus;

			passed &= passed - 1;
		}
	}

	void pawn_base(const board &pos, pawn::hash &entry, uint64 attacks[][6])
	{
		// evaluating everything pawn-related that can be stored in the pawn hash table

		for (int color{ WHITE }; color <= BLACK; ++color)
		{
			auto xcolor{ color ^ 1 };
			auto pawns{ pos.pieces[PAWNS] & pos.side[color] };
			auto pawns_friend{ pawns };

			while (pawns)
			{
				auto sq{ static_cast<int>(bit::scan(pawns)) };
				auto sq_bit{ 1ULL << sq };
				auto sq_stop{ bit::shift(sq_bit, shift::push[color]) };
				auto file{ index::file(sq) };
				auto rank{ index::rank(sq) };

				// PSQT

				entry.score[MG][color] += pawn_psq[xcolor][MG][sq] + piece_value[MG][PAWNS];
				entry.score[EG][color] += pawn_psq[xcolor][EG][sq] + piece_value[EG][PAWNS];

				// penalizing isolated pawns

				if (!(adjacent[file] & pawns_friend))
				{
					entry.score[MG][color] += isolated[MG];
					entry.score[EG][color] += isolated[EG];
				}

				// penalizing backward pawns

				if ((attacks[xcolor][PAWNS] & sq_stop) && !((front_span[xcolor][bit::scan(sq_stop)] & pawns_friend) ^ sq_bit))
				{
					entry.score[MG][color] += backward[MG];
					entry.score[EG][color] += backward[EG];
				}

				// rewarding connected pawns

				else if (adjacent[file] & pawns_friend & (bit::rank[rank] | bit::shift(bit::rank[rank], shift::push[xcolor])))
				{
					entry.score[MG][color] += connected[color][MG][rank];
					entry.score[EG][color] += connected[color][EG][rank];
				}

				// finding passed pawns

				if (path::is_passed(pos, sq, color, xcolor, file))
					entry.passed[color] |= sq_bit;

				pawns &= pawns - 1;
			}
		}
	}

	void pawns(const board &pos, int sum[][2][2], uint64 attacks[][6], uint64 attacks_final[], pawn &hash)
	{
		// evaluating pawns

		if (!pos.pieces[PAWNS]) return;
		assert(pos.pawn_key != 0ULL);

		// storing into & probing the pawn hash table
		// the table provides only some basic evaluation

		pawn::hash new_entry{};
		pawn::hash &entry{ hash.table ? hash.table[pos.pawn_key & pawn::mask] : new_entry };
		if (entry.key != pos.pawn_key)
		{
			entry = pawn::hash{};
			pawn_base(pos, entry, attacks);
			entry.key = pos.pawn_key;
		}

		// extending the pawn evaluation

		pawn_extended(pos, sum, attacks_final, entry, WHITE);
		pawn_extended(pos, sum, attacks_final, entry, BLACK);
	}

	void pieces(const board &pos, int sum[][2][2], uint64 attacks[][6], int &phase, int color)
	{
		// evaluating all pieces except pawns

		auto xcolor{ color ^ 1 };

		// initializing threats against the king

		auto king_zone{ attack::king_map[pos.sq_king[xcolor]] | bit::shift(attack::king_map[pos.sq_king[xcolor]], shift::push[xcolor]) };
		struct attack_threat
		{
			int count{};
			int sum{};
		} threat;

		// finding all pins to restrict piece mobility

		uint64 pin_moves[64]{};
		attack::pins(pos, color, color, pin_moves);

		// starting with the evaluation
		// knights

		auto pieces{ pos.side[color] & pos.pieces[KNIGHTS] };
		while (pieces)
		{
			auto sq{ bit::scan(pieces) };

			sum[KNIGHTS][MG][color] += knight_psq[xcolor][MG][sq] + piece_value[MG][KNIGHTS];
			sum[KNIGHTS][EG][color] += knight_psq[xcolor][EG][sq] + piece_value[EG][KNIGHTS];

			// generating attacks


			auto targets{ attack::knight_map[sq] & ~pin_moves[sq] };
			attacks[color][KNIGHTS] |= targets;

			targets &= ~((attacks[xcolor][PAWNS] & ~pos.side[BOTH]) | (pos.side[color] & pos.pieces[PAWNS]));
			king_threat::add(KNIGHTS, targets, king_zone, threat.count, threat.sum);

			// rewarding outposts

			if ((1ULL << sq) & outpost_zone[color] & attacks[color][PAWNS])
			{
				auto weight{ 1 };

				if (!(front_span[color][sq] & ~bit::file[index::file(sq)] & pos.pieces[PAWNS] & pos.side[xcolor]))
					weight += 3;

				sum[KNIGHTS][MG][color] += knight_outpost[MG] * weight;
				sum[KNIGHTS][EG][color] += knight_outpost[EG] * weight;
			}

			// mobility

			auto count{ bit::popcnt(targets) };
			sum[MOBILITY][MG][color] += knight_mobility[MG][count];
			sum[MOBILITY][EG][color] += knight_mobility[EG][count];

			phase += phase_value[KNIGHTS];
			pieces &= pieces - 1;
		}

		// bishops

		pieces = pos.side[color] & pos.pieces[BISHOPS];
		while (pieces)
		{
			auto sq{ bit::scan(pieces) };

			sum[BISHOPS][MG][color] += bishop_psq[xcolor][MG][sq] + piece_value[MG][BISHOPS];
			sum[BISHOPS][EG][color] += bishop_psq[xcolor][EG][sq] + piece_value[EG][BISHOPS];

			// generating attacks

			auto targets{ attack::by_slider<BISHOP>(sq, pos.side[BOTH] & ~(pos.pieces[QUEENS] & pos.side[color])) & ~pin_moves[sq] };
			attacks[color][BISHOPS] |= targets;

			targets &= ~(pos.side[color] & pos.pieces[PAWNS]);
			king_threat::add(BISHOPS, targets, king_zone, threat.count, threat.sum);

			// bishop pair bonus

			if (pieces & (pieces - 1))
			{
				sum[BISHOPS][MG][color] += bishop_pair[MG];
				sum[BISHOPS][EG][color] += bishop_pair[EG];
			}

			// mobility

			auto count{ bit::popcnt(targets) };
			sum[MOBILITY][MG][color] += bishop_mobility[MG][count];
			sum[MOBILITY][EG][color] += bishop_mobility[EG][count];

			phase += phase_value[BISHOPS];
			pieces &= pieces - 1;
		}

		// rooks

		pieces = pos.side[color] & pos.pieces[ROOKS];
		while (pieces)
		{
			auto sq{ bit::scan(pieces) };

			sum[ROOKS][MG][color] += rook_psq[xcolor][MG][sq] + piece_value[MG][ROOKS];
			sum[ROOKS][EG][color] += rook_psq[xcolor][EG][sq] + piece_value[EG][ROOKS];

			// generating attacks

			auto targets{ attack::by_slider<ROOK>(sq, pos.side[BOTH] & ~((pos.pieces[QUEENS] | pos.pieces[ROOKS]) & pos.side[color]))
				& ~pin_moves[sq] };
			attacks[color][ROOKS] |= targets;

			targets &= ~(pos.side[color] & pos.pieces[PAWNS]);
			king_threat::add(ROOKS, targets, king_zone, threat.count, threat.sum);

			// being on a open or semi-open file

			if (!(bit::file[index::file(sq)] & pos.pieces[PAWNS] & pos.side[color]))
			{
				auto weight{ 1 };

				if (!(bit::file[index::file(sq)] & pos.pieces[PAWNS]))
					weight += 1;
				if (bit::file[index::file(sq)] & pos.pieces[KINGS] & pos.side[xcolor])
					weight += 1;

				sum[ROOKS][MG][color] += rook_open_file[MG] * weight;
				sum[ROOKS][EG][color] += rook_open_file[EG] * weight;

			}

			// being on the 7th rank

			if (relative::rank(index::rank(sq), color) == R7)
			{
				if (bit::rank[relative::rank(R7, color)] & pos.pieces[PAWNS] & pos.side[xcolor]
					|| bit::rank[relative::rank(R8, color)] & pos.pieces[KINGS] & pos.side[xcolor])
				{
					sum[ROOKS][MG][color] += major_on_7th[MG];
					sum[ROOKS][EG][color] += major_on_7th[EG];
				}
			}

			// mobility

			auto count{ bit::popcnt(targets) };
			sum[MOBILITY][MG][color] += rook_mobility[MG][count];
			sum[MOBILITY][EG][color] += rook_mobility[EG][count];

			phase += phase_value[ROOKS];
			pieces &= pieces - 1;
		}

		// queens

		pieces = pos.side[color] & pos.pieces[QUEENS];
		while (pieces)
		{
			auto sq{ bit::scan(pieces) };

			sum[QUEENS][MG][color] += queen_psq[xcolor][MG][sq] + piece_value[MG][QUEENS];
			sum[QUEENS][EG][color] += queen_psq[xcolor][EG][sq] + piece_value[EG][QUEENS];

			// generating attacks

			auto targets{ attack::by_slider<QUEEN>(sq, pos.side[BOTH]) & ~pin_moves[sq] };
			attacks[color][QUEENS] |= targets;

			targets &= ~(pos.side[color] & pos.pieces[PAWNS]);
			king_threat::add(QUEENS, targets, king_zone, threat.count, threat.sum);

			// being on the 7th rank

			if (relative::rank(index::rank(sq), color) == R7)
			{
				if (bit::rank[relative::rank(R7, color)] & pos.pieces[PAWNS] & pos.side[xcolor]
					|| bit::rank[relative::rank(R8, color)] & pos.pieces[KINGS] & pos.side[xcolor])
				{
					sum[QUEENS][MG][color] += major_on_7th[MG];
					sum[QUEENS][EG][color] += major_on_7th[EG];
				}
			}

			// mobility

			auto count{ bit::popcnt(targets) };
			sum[MOBILITY][MG][color] += queen_mobility[MG][count];
			sum[MOBILITY][EG][color] += queen_mobility[EG][count];

			phase += phase_value[QUEENS];
			pieces &= pieces - 1;
		}

		// king

		{
			sum[KINGS][MG][color] += king_psq[xcolor][MG][pos.sq_king[color]] + piece_value[MG][KINGS];
			sum[KINGS][EG][color] += king_psq[xcolor][EG][pos.sq_king[color]] + piece_value[EG][KINGS];

			// generating attacks

			attacks[color][KINGS] |= attack::king_map[pos.sq_king[color]];

			// king safety threats
			// credits go to https://chessprogramming.wikispaces.com/King+Safety

			if (threat.count >= 2)
			{
				auto weight{ std::min(threat.sum, 63) };

				if (!(pos.pieces[QUEENS] & pos.side[color]))
					weight /= 2;
				if (!(pos.pieces[ROOKS]  & pos.side[color]))
					weight -= weight / 5;

				sum[KINGS][MG][xcolor] -= king_threat[weight];
				sum[KINGS][EG][xcolor] -= king_threat[weight];
			}
		}
	}

	void evaluate(const board &pos, int sum[][2][2], int &phase, pawn &hash)
	{
		// initializing attack tables

		uint64 attacks[2][6]{};
		attacks[WHITE][PAWNS] = attack::by_pawns(pos, WHITE);
		attacks[BLACK][PAWNS] = attack::by_pawns(pos, BLACK);

		// evaluating pieces & threats against the king

		pieces(pos, sum, attacks, phase, WHITE);
		pieces(pos, sum, attacks, phase, BLACK);

		// evaluating threats against pieces & pawns

		uint64 attacks_final[2]{};
		for (auto &att_piece : attacks[WHITE]) attacks_final[WHITE] |= att_piece;
		for (auto &att_piece : attacks[BLACK]) attacks_final[BLACK] |= att_piece;

		threats(pos, sum, attacks, attacks_final, WHITE);
		threats(pos, sum, attacks, attacks_final, BLACK);

		// evaluating pawns
		 
		pawns(pos, sum, attacks, attacks_final, hash);
	}
}

int eval::static_eval(const board &pos, pawn &hash)
{
	// entry point of the evaluation chain
	// filtering out obviously drawn positions first

	if (draw::obvious(pos))
		return uci::contempt[pos.xturn];

	// evaluating

	int phase{};
	int sum[10][2][2]{};

	evaluate(pos, sum, phase, hash);
	auto score{ score::interpolate(sum, phase) };
	assert(std::abs(score) < SCORE_LONGEST_MATE);

	// scaling drawish positions

	return relative::side[pos.turn] * draw::scale(pos, score);
}

void eval::itemise_eval(const board &pos, pawn &hash)
{
	// itemizing & displaying the static evaluation for debugging

	int phase{};
	int sum[10][2][2]{};
	sum::material(sum);
	evaluate(pos, sum, phase, hash);

	// displaying the results

	auto label{ "|            | w[MG]  w[EG] | b[MG]  b[EG] |  [MG]   [EG] |\n" };
	auto row  { "+------------+--------------+--------------+--------------+\n" };
	sync::cout << row << label;
	assert(feature.size() == 10);

	for (uint32 f{}; f < feature.size(); ++f)
	{
		sync::cout << "|" << feature[f] << std::string(12 - feature[f].size(), ' ') << "|";
		for (int color{ WHITE }; color <= BLACK; ++color)
		{
			for (int stage{ MG }; stage <= EG; ++stage)
			{
				if (f == MATERIAL)
				{
					sync::cout << "  ---- ";
					continue;
				}

				// adjusting the material score

				int diff{};
				if (f <= QUEENS)
				{
					diff = bit::popcnt(pos.pieces[f] & pos.side[color]) * piece_value[stage][f];
					sum[MATERIAL][stage][color] += diff;
				}
				sum[f][stage][color] -= diff;
				output::align(sum[f][stage][color]);
			}
			sync::cout << "|";
		}
		output::align(sum[f][MG][WHITE] - sum[f][MG][BLACK]);
		output::align(sum[f][EG][WHITE] - sum[f][EG][BLACK]);
		sync::cout << "|\n";
	}

	// summing up & considering draws

	auto score{ score::interpolate(sum, phase) };
	auto scale{ draw::scale(pos, score) };

	sync::cout << row;
	sync::cout.precision(2);
	sync::cout << score / 100.0 << std::endl;

	if (draw::obvious(pos))
		sync::cout << uci::contempt[pos.xturn] / 100.0 << " (draw)" << std::endl;
	else if (scale != score)
		sync::cout << scale / 100.0 << " (draw scaling)" << std::endl;
}

void eval::mirror_tables()
{
	// mirroring various tables to black's perspective

	for (int s{ MG }; s <= EG; ++s)
	{
		for (int sq{ H1 }; sq <= A8; ++sq)
		{
			pawn_psq[  BLACK][s][63 - sq] = pawn_psq[  WHITE][s][sq];
			knight_psq[BLACK][s][63 - sq] = knight_psq[WHITE][s][sq];
			bishop_psq[BLACK][s][63 - sq] = bishop_psq[WHITE][s][sq];
			rook_psq[  BLACK][s][63 - sq] = rook_psq[  WHITE][s][sq];
			queen_psq[ BLACK][s][63 - sq] = queen_psq[ WHITE][s][sq];
			king_psq[  BLACK][s][63 - sq] = king_psq[  WHITE][s][sq];
		}
	}

	for (int r{ R1 }; r <= R8; ++r)
	{
		passed_rank[BLACK][r] = passed_rank[WHITE][R8 - r];
		shield_rank[BLACK][r] = shield_rank[WHITE][R8 - r];
		storm_rank[ BLACK][r] = storm_rank[ WHITE][R8 - r];

		connected[BLACK][MG][r] = connected[WHITE][MG][R8 - r];
		connected[BLACK][EG][r] = connected[WHITE][EG][R8 - r];
	}
}

void eval::fill_tables()
{
	// filling various tables at startup

	mirror_tables();

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
		auto f{ index::file(sq) };
		auto file_span{ bit::file[f] | adjacent[f] };

		front_span[WHITE][sq] = file_span & attack::in_front[WHITE][sq];
		front_span[BLACK][sq] = file_span & attack::in_front[BLACK][sq];
	}
}
