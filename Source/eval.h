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


#pragma once

#include <array>
#include <vector>

#include "zobrist.h"
#include "board.h"
#include "types.h"

// packing and unpacking evaluation scores

constexpr int S(int mid, int end) { return (int(uint32(mid) << 16) + end); }
constexpr int S_MG(int sc) { return int(int16(uint16(unsigned((sc)+0x8000) >> 16))); }
constexpr int S_EG(int sc) { return int(int16(uint16(unsigned(sc)))); }

// evaluating a position

class kingpawn_hash;

namespace eval
{
	void  mirror_tables();
	score static_eval(const board& pos, kingpawn_hash& hash);

	// material weights

	inline std::array<int, 6> piece_value
	{ { S( 77, 100), S(344, 347), S(360, 380), S(480, 639), S(1042, 1227), S( 0, 0) } };

	// material imbalance weights

	inline int tempo{ 19 };
	inline std::array<int, 6> complexity
	{ { S(0, 6), S(0, 1), S(0, 16), S(0, 40), S(0, -17), S(0,-55) } };

	inline std::array<int, 3> scale_few_pawns{ { 2, 11, 15 } };
	inline int bishop_pair       { S(19, 61) };
	inline int bishop_color_pawns{ S(-5, -6) };

	// minor piece weights

	inline int bishop_trapped{ S(-9, -9) };
	inline int knight_outpost{ S( 7,  5) };
	inline std::array<int, 4> knight_distance_kings{ { S(-16, 0), S(-15, 0), S(-24, 0), S(-60, 0) }};

	// major piece weights

	inline int rook_on_7th   { S(-11, 26) };
	inline int rook_open_file{ S( 13,  4) };

	// piece threatening weights

	inline int threat_pawn { S(  5, -22) };
	inline int threat_minor{ S(-15, -30) };
	inline int threat_rook { S(-49, -21) };
	inline int threat_queen_by_minor{ S(-36, -38) };
	inline int threat_queen_by_rook { S(-43, -40) };
	inline int threat_piece_by_pawn { S(-53, -24) };
	inline int threat_piece_by_king { S(-17, -20) };

	// king threatening weights

	inline int weak_king_sq{ S(20, 0) };
	inline std::array<int,  5> threat_king_by_check{ { S(0, 0), S(46, 0), S(19, 0), S(67, 0), S(34, 0) } };
	inline std::array<int,  5> threat_king_by_xray { { S(0, 0), S( 0, 0), S(60, 0), S(85, 0), S(39, 0) } };
	inline std::array<int,  5> threat_king_weight  { { 0, 4, 3, 4, 4 } };
	inline std::array<int, 60> threat_king_sum
	{ {
		S(  0, 0), S(  0, 0), S(  0, 0), S(  0, 0), S(  0, 0), S(  0, 0),
		S(  0, 0), S( -7, 0), S( -8, 0), S( -8, 0), S( -1, 0), S(  4, 0),
		S(  6, 0), S( 27, 0), S( 18, 0), S( 20, 0), S( 20, 0), S( 38, 0),
		S( 49, 0), S( 44, 0), S( 39, 0), S( 80, 0), S( 73, 0), S( 87, 0),
		S( 71, 0), S(112, 0), S(115, 0), S( 97, 0), S(120, 0), S(133, 0),
		S(162, 0), S(163, 0), S(172, 0), S(205, 0), S(220, 0), S(217, 0),
		S(195, 0), S(167, 0), S(310, 0), S(228, 0), S(259, 0), S(351, 0),
		S(315, 0), S(259, 0), S(297, 0), S(346, 0), S(244, 0), S(472, 0),
		S(361, 0), S(459, 0), S(460, 0), S(531, 0), S(299, 0), S(674, 0),
		S(660, 0), S(548, 0), S(424, 0), S(500, 0), S(985, 0), S(973, 0)
	} };

	// pawn weights

	inline int isolated             { S( -9, -8) };
	inline int backward             { S( -4, -5) };
	inline int king_without_pawns   { S(-27,-51) };
	inline int king_dist_passed_cl  { S( -5, -8) };
	inline int king_dist_passed_cl_x{ S( -1, 22) };

	inline std::array<std::array<int, 8>, 2> connect_rank
	{ {
		{{ S(0, 0), S(  3, 1), S( 14,10), S( 12, 9), S( 21,17), S( 38,49), S(150,19), S(  0, 0) }}
	} };
	inline std::array<std::array<int, 8>, 2> passed_rank
	{ {
		{{ S(0, 0), S(-23, 0), S(-19, 0), S( 16, 0), S( 72, 0), S(164, 0), S(307, 0), S(  0, 0) }}
	} };
	inline std::array<std::array<int, 8>, 2> shield_rank
	{ {
		{{ S(0, 0), S( -2, 0), S( -6, 0), S(-12, 0), S(-16, 0), S(  2, 0), S(  3, 0), S(-23, 0) }}
	} };

	// mobility weights

	inline std::array<int, 9>knight_mobility
	{ { S( -49, -61), S( -24, -34), S( -13,   2), S(   0,  14), S(   6,  25), S(  12,  35),
		S(  21,  37), S(  30,  38), S(  36,  34) } };

	inline std::array<int, 14> bishop_mobility
	{ { S( -32, -39), S(   1, -31), S(  16,  -3), S(  24,  15), S(  31,  22), S(  35,  30),
		S(  36,  36), S(  36,  40), S(  37,  43), S(  38,  44), S(  40,  42), S(  45,  36),
		S(  60,  36), S(  85,  18) }};

	inline std::array<int, 15> rook_mobility
	{ { S( -70,-111), S( -42, -56), S(  -9, -22), S(   0,   0), S(  -1,  13), S(  -2,  24),
		S(   0,  30), S(   3,  33), S(   9,  36), S(  12,  40), S(  15,  45), S(  18,  47),
		S(  21,  50), S(  26,  49), S(  26,  54) }};

	inline std::array<int, 28> queen_mobility
	{ { S(-292,-199), S(-185,-112), S( -69, -19), S( -28, -56), S(  -8, -96), S(  -4, -46),
		S(   2, -26), S(   2,  -5), S(   8,  -5), S(   9,  10), S(  14,   9), S(  16,  18),
		S(  18,  21), S(  20,  28), S(  20,  31), S(  19,  38), S(  18,  45), S(  19,  45),
		S(   9,  58), S(  14,  53), S(  18,  53), S(  24,  41), S(  19,  48), S(   2,  59),
		S(   9,  64), S(  63,  16), S(  39,  18), S( 141,  19) } };

	// piece square tables

	inline std::array<std::array<int, 64>, 2> pawn_psq
	{ { {},
	{{
		S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0),
		S( -59,  18), S( -57,   8), S( -16, -21), S(  17, -51), S(  17, -51), S( -16, -21), S( -57,   8), S( -59,  18),
		S(  -9,  30), S(  -7,  24), S(  20,  -1), S(  13, -24), S(  13, -24), S(  20,  -1), S(  -7,  24), S(  -9,  30),
		S(  -9,  16), S( -12,  11), S(  -2,   4), S(   7,  -8), S(   7,  -8), S(  -2,   4), S( -12,  11), S(  -9,  16),
		S( -10,   7), S(  -9,   6), S(   5,   2), S(  11,   0), S(  11,   0), S(   5,   2), S(  -9,   6), S( -10,   7),
		S( -14,  -2), S(  -3,  -3), S( -10,   4), S(  -5,   7), S(  -5,   7), S( -10,   4), S(  -3,  -3), S( -14,  -2),
		S(  -8,   1), S(   7,   3), S(   7,  11), S( -11,  14), S( -11,  14), S(   7,  11), S(   7,   3), S(  -8,   1),
		S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0)
	}}
	} };

	inline std::array<std::array<int, 64>, 2> knight_psq
	{ { {},
    {{
        S(-125, -35), S( -77,   3), S( -78,  11), S( -53,  12), S( -53,  12), S( -78,  11), S( -77,   3), S(-125, -35),
        S( -11,   1), S( -13,  18), S(  24,   5), S(  13,  18), S(  13,  18), S(  24,   5), S( -13,  18), S( -11,   1),
        S(   7,   2), S(  16,  14), S(  24,  20), S(  20,  18), S(  20,  18), S(  24,  20), S(  16,  14), S(   7,   2),
        S(  30,  16), S(  13,  21), S(  23,  25), S(  12,  30), S(  12,  30), S(  23,  25), S(  13,  21), S(  30,  16),
        S(  16,  15), S(  14,  17), S(  11,  22), S(  13,  27), S(  13,  27), S(  11,  22), S(  14,  17), S(  16,  15),
        S(  -3,   5), S(  10,   7), S(   6,   6), S(   6,  23), S(   6,  23), S(   6,   6), S(  10,   7), S(  -3,   5),
        S(  -8,  19), S(   5,   8), S(  -3,   3), S(   3,   9), S(   3,   9), S(  -3,   3), S(   5,   8), S(  -8,  19),
        S( -37,   3), S(  -8,   4), S( -10,   2), S( -13,   8), S( -13,   8), S( -10,   2), S(  -8,   4), S( -37,   3),
    }}
	} };

	inline std::array<std::array<int, 64>, 2> bishop_psq
	{ { {},
    {{
        S( -60,   5), S( -82,  14), S( -67,   2), S(-112,  16), S(-112,  16), S( -67,   2), S( -82,  14), S( -60,   5),
        S( -43,  -2), S( -51,   3), S( -28,   4), S( -46,   3), S( -46,   3), S( -28,   4), S( -51,   3), S( -43,  -2),
        S(  -1,  -5), S(  -4,   8), S(   5,   8), S(   7,   0), S(   7,   0), S(   5,   8), S(  -4,   8), S(  -1,  -5),
        S( -22,   3), S(  -3,   6), S(  -2,   9), S(  13,  15), S(  13,  15), S(  -2,   9), S(  -3,   6), S( -22,   3),
        S(  -2,  -9), S(  -8,  -2), S(  -4,   7), S(  15,  10), S(  15,  10), S(  -4,   7), S(  -8,  -2), S(  -2,  -9),
        S(   0,  -8), S(   7,  -2), S(   3,   5), S(   5,   7), S(   5,   7), S(   3,   5), S(   7,  -2), S(   0,  -8),
        S(  14, -14), S(  12,  -6), S(  12,  -5), S(   0,  -3), S(   0,  -3), S(  12,  -5), S(  12,  -6), S(  14, -14),
        S(   0,  -7), S(   6, -10), S( -13,   4), S(  -6,  -1), S(  -6,  -1), S( -13,   4), S(   6, -10), S(   0,  -7),
    }}
	} };

	inline std::array<std::array<int, 64>, 2> rook_psq
	{ { {},
    {{
        S(  10,  13), S( -12,  28), S(  12,  19), S( -16,  28), S( -16,  28), S(  12,  19), S( -12,  28), S(  10,  13),
        S(   8,   3), S( -10,  18), S(   7,  15), S(  14,  11), S(  14,  11), S(   7,  15), S( -10,  18), S(   8,   3),
        S(   0,  14), S(  14,  22), S(   2,  26), S(  25,  13), S(  25,  13), S(   2,  26), S(  14,  22), S(   0,  14),
        S(  -3,  17), S(  -1,  24), S(   1,  20), S(   9,  16), S(   9,  16), S(   1,  20), S(  -1,  24), S(  -3,  17),
        S( -14,   8), S(  -9,  11), S( -16,  15), S( -13,  13), S( -13,  13), S( -16,  15), S(  -9,  11), S( -14,   8),
        S( -24,  -2), S(  -3,  -3), S( -13,   0), S( -10,  -3), S( -10,  -3), S( -13,   0), S(  -3,  -3), S( -24,  -2),
        S( -38,  -6), S( -11,  -9), S(  -6,  -8), S(  -2,  -9), S(  -2,  -9), S(  -6,  -8), S( -11,  -9), S( -38,  -6),
        S( -10, -11), S( -12,  -9), S(  -2,  -8), S(   2, -13), S(   2, -13), S(  -2,  -8), S( -12,  -9), S( -10, -11),
    }}
	} };

	inline std::array<std::array<int, 64>, 2> queen_psq
	{ { {},
    {{
        S( -19,   0), S(  -1,  19), S(  10,  23), S(   2,  31), S(   2,  31), S(  10,  23), S(  -1,  19), S( -19,   0),
        S(  20, -11), S( -33,  41), S( -11,  61), S( -28,  74), S( -28,  74), S( -11,  61), S( -33,  41), S(  20, -11),
        S(  26,   5), S(  17,  26), S(  -8,  71), S(   0,  66), S(   0,  66), S(  -8,  71), S(  17,  26), S(  26,   5),
        S(  12,  33), S(  -1,  56), S(  -4,  61), S( -13,  76), S( -13,  76), S(  -4,  61), S(  -1,  56), S(  12,  33),
        S(   3,  30), S(   8,  39), S(   0,  36), S(  -2,  53), S(  -2,  53), S(   0,  36), S(   8,  39), S(   3,  30),
        S(  12,  -1), S(  17,  11), S(   7,  29), S(   4,  23), S(   4,  23), S(   7,  29), S(  17,  11), S(  12,  -1),
        S(  16, -20), S(  20, -18), S(  22, -20), S(  16,  -2), S(  16,  -2), S(  22, -20), S(  20, -18), S(  16, -20),
        S(  21, -35), S(   5, -10), S(   9, -12), S(  17, -22), S(  17, -22), S(   9, -12), S(   5, -10), S(  21, -35),
    }}
	} };

	inline std::array<std::array<int, 64>, 2> king_psq
	{ { {},
    {{
        S( -37,-171), S(  79, -72), S(   3, -32), S(  22, -13), S(  22, -13), S(   3, -32), S(  79, -72), S( -37,-171),
        S( -53, -12), S(  33,  17), S(  47,  24), S(  32,  22), S(  32,  22), S(  47,  24), S(  33,  17), S( -53, -12),
        S( -70,   1), S(  25,  28), S(  21,  39), S(  -8,  44), S(  -8,  44), S(  21,  39), S(  25,  28), S( -70,   1),
        S(-118,   4), S( -53,  27), S( -66,  42), S( -66,  48), S( -66,  48), S( -66,  42), S( -53,  27), S(-118,   4),
        S(-136,  -2), S( -72,  15), S( -57,  26), S( -66,  36), S( -66,  36), S( -57,  26), S( -72,  15), S(-136,  -2),
        S( -36, -26), S(   1, -13), S( -26,   4), S( -33,  15), S( -33,  15), S( -26,   4), S(   1, -13), S( -36, -26),
        S(  27, -48), S(  30, -31), S(  -1, -14), S( -19,  -6), S( -19,  -6), S(  -1, -14), S(  30, -31), S(  27, -48),
        S(  27, -89), S(  48, -57), S(  17, -38), S(  20, -44), S(  20, -44), S(  17, -38), S(  48, -57), S(  27, -89),
    }}
	} };
}

// managing the king-pawn hash table which speeds up the evaluation function

class kingpawn_hash
{
private:
	// size of 1 << 11 correlates to a fixed pawn-hash table of ~98 KB per thread

	constexpr static std::size_t size{ 1U << 11 };
	constexpr static key64 mask{ size - 1 };

public:
	enum table_memory
	{
		ALLOCATE,
		ALLOCATE_NONE
	};

	// king-pawn hash entry is 48 bytes

	struct hash
	{
		key64 key{};
		std::array<bit64, 2> passed{};
		std::array<bit64, 2> attack{};
		std::array<int32, 2> score{};
	} entry;

	static_assert(sizeof(hash) == 48);

	// actual table

	std::vector<hash> table{};

	hash& get_entry(const board& pos)
	{
		return !table.empty() && pos.pieces[PAWN] ? table[zobrist::kingpawn_key(pos) & mask] : entry;
	}

	kingpawn_hash(table_memory memory)
	{
		if (memory == ALLOCATE)
		{
			table.resize(size);
			for (auto& t : table) t = hash{};
		}
	}
};