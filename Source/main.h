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


#pragma once

// globally needed libraries

#include <vector>
#include <cmath>
#include <cassert>
#include <limits>
#include <iostream>
#include <string>
#include <algorithm>

// defining runtime-expensive debug assertions

#if defined(DEBUG_EXP)
  #define assert_exp(x) assert(x)
#else
  #define assert_exp(x) ((void)0)
#endif

// type definitions

typedef unsigned char uint8;
typedef signed   char  int8;

typedef unsigned short uint16;
typedef signed   short  int16;

typedef unsigned int uint32;
typedef signed   int  int32;

typedef unsigned long long uint64;
typedef signed   long long  int64;

static_assert(sizeof(uint8)  == 1, "char != 1 byte");
static_assert(sizeof(uint16) == 2, "short != 2 bytes");
static_assert(sizeof(uint32) == 4, "int != 4 bytes");
static_assert(sizeof(uint64) == 8, "long long != 8 bytes");

// limiting constants

namespace lim
{
	constexpr int64 nodes   { std::numeric_limits<int64>::max() };
	constexpr int64 movetime{ std::numeric_limits<int64>::max() };

	constexpr int threads{ 16 };
	constexpr int depth{ 110 };
	constexpr int dtz{ 1048 };
	constexpr int moves{ 256 };
	constexpr int multipv{ moves };
	constexpr int hash{ 16384 };
	constexpr int overhead{ 5000 };
	constexpr int min_contempt{ -100 };
	constexpr int max_contempt{  100 };
	constexpr int syzygy_pieces{ 6 };
}

// globally needed enumerators

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

enum file_index{ H, G, F, E, D, C, B, A };

enum rank_index{ R1, R2, R3, R4, R5, R6, R7, R8 };

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

enum eval_index
{
	MATERIAL = 6,
	MOBILITY = 7,
	PASSED = 8,
	THREATS = 9
};

enum pv_index { PREVIOUS = lim::depth };

enum side_index
{
	WHITE,
	BLACK,
	BOTH
};

enum castling_side
{
	CASTLE_SHORT = 10,
	CASTLE_LONG = 11
};

enum castling_square{ PROHIBITED = 64 };

enum promo_mode
{
	PROMO_ALL = 12,
	PROMO_KNIGHT = 12,
	PROMO_BISHOP = 13,
	PROMO_ROOK = 14,
	PROMO_QUEEN = 15
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
	DEFERRED,
	TACTICAL,
	CHECK,
	EVASION
};

enum game_stage{ MG, EG };

enum sliding_type
{
	ROOK,
	BISHOP,
	QUEEN
};

enum score_type
{
	SCORE_NONE         = -32100,
	SCORE_CURSED_WIN   =  2,
	SCORE_DRAW         =  0,
	SCORE_BLESSED_LOSS = -2,
	SCORE_MATE         =  32000,
	SCORE_LONGEST_MATE =  28000,
	SCORE_TB           =  16000,
	SCORE_TBMATE       =  16000
};

enum move_type{ MOVE_NONE };

enum bound_type
{
	EXACT = 1,
	UPPER = 2,
	LOWER = 3
};

enum exception_type{ STOP_SEARCHING = 1 };

enum index_type{ MAIN };
