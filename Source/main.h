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


#pragma once

// debug switches

#define POPCNT
#define NDEBUG
#define NDEBUG_EXP
#define NLOG

// global libraries

#include <iostream>
#include <string>
#include <algorithm>
#include <cassert>

// defining runtime-expensive assertions

#if defined(DEBUG_EXP)
#define assert_exp(x) assert(x)
#else
#define assert_exp(x) ((void)0)
#endif

// global enumerators

enum square_index
{
	H1, G1, F1, E1, D1, C1, B1, A1,
	H2, G2, F2, E2, D2, C2, B2, A2,
	H3, G3, F3, E3, D3, C3, B3, A3,
	H4, G4, F4, E4, D4, C4, B4, A4,
	H5, G5, F5, E5, D5, C5, B5, A5,
	H6, G6, F6, E6, D6, C6, B6, A6,
	H7, G7, F7, E7, D7, C7, B7, A7,
	H8, G8, F8, E8, D8, C8, B8, A8
};

enum file_index
{
	H, G, F, E, D, C, B, A
};

enum rank_index
{
	R1, R2, R3, R4, R5, R6, R7, R8
};

enum piece_index
{
	PAWNS = 0,
	KNIGHTS = 1,
	BISHOPS = 2,
	ROOKS = 3,
	QUEENS = 4,
	KINGS = 5,
	NONE = 7
};

enum pawn_move_index
{
	DOUBLEPUSH = 8,
	ENPASSANT = 9
};

enum castling
{
	SHORT = 10,
	LONG = 11
};

enum castling_square
{
	PROHIBITED = 64
};

enum promo_mode
{
	PROMO_ALL = 12,
	PROMO_KNIGHT = 12,
	PROMO_BISHOP = 13,
	PROMO_ROOK = 14,
	PROMO_QUEEN = 15
};

enum eval_index
{
	MATERIAL = 6,
	MOBILITY = 7,
	PASSED = 8
};

enum sliding_type
{
	ROOK,
	BISHOP,
	QUEEN
};

enum side_index
{
	WHITE,
	BLACK,
	BOTH
};

enum gen_mode
{
	PSEUDO,
	LEGAL
};

enum gen_stage
{
	HASH,
	WINNING,
	KILLER,
	QUIET,
	LOOSING,
	TACTICAL,
	CHECK,
	EVASION
};

enum game_stage
{
	MG,
	EG
};

enum score_type
{
	NO_SCORE  = -30000,
	MIN_SCORE = -20000,
	MAX_SCORE =  20000,
	MATE_SCORE = 18951,
};

enum move_type
{
	NO_MOVE
};

enum bound_type
{
	EXACT = 1,
	UPPER = 2,
	LOWER = 3
};

// types

typedef unsigned long long uint64;
typedef unsigned int uint32;
typedef unsigned short uint16;
typedef unsigned char uint8;

typedef signed short int16;

// limiting constants

namespace lim
{
	const uint64 nodes{ ~0ULL };
	const uint64 movetime{ ~0ULL };

	const int depth{ 128 };

	const int period{ depth + 128 };
	const int moves{ 256 };
	const int multipv{ moves };

	const int hash{ 4096 };
	const int overhead{ 1000 };
	const int min_contempt{ -100 };
	const int max_contempt{  100 };
}

// relativating white's perspective

namespace relative
{
	inline int rank(int rank, int turn)
	{
		return turn == WHITE ? rank : 7 - rank;
	}
}

// square functions

namespace square
{
	const uint64 white{ 0xaa55aa55aa55aa55 };
	const uint64 black{ 0x55aa55aa55aa55aa };

	inline int file(int sq)
	{
		return sq & 7;
	}

	inline int rank(int sq)
	{
		return sq >> 3;
	}

	inline int distance(int sq1, int sq2)
	{
		return std::max(abs(file(sq1) - file(sq2)), abs(rank(sq1) - rank(sq2)));
	}
}