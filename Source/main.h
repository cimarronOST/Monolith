/*
  Monolith 0.1  Copyright (C) 2017 Jonas Mayr

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

//// compile controls

#define SSE4
#define DEBUG
#define LOG_ON

//// global includes

#include <iostream>
#include <string>
#include <cassert>

//// defines

#ifdef LOG_ON
#define log sync_log
#else
#define log std
#endif

//// used namespaces

using std::endl;
using std::string;

//// global strings

const string version{ "0.1" };
const string startpos{ "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 0" };

//// typedefs

typedef unsigned long long uint64;
typedef unsigned int uint32;
typedef unsigned short uint16;
typedef unsigned char uint8;
typedef signed short int16;

//// global constants

const uint8 castl_right[]{ 0x1, 0x4, 0x10, 0x40 };

const int value[]{ 100, 500, 320, 330, 950 };

namespace lim
{
	const int movetime{ 0x7fffffff };
	const int depth{ 64 };
	const int period{ 1024 };
	const int movegen{ 256 };
	const int hash{ 1024 };
}

//// global functions

inline uint64 shift(uint64 bb, int shift)
{
	return (bb << shift) | (bb >> (64 - shift));
}
inline uint16 encode(uint32 sq1, uint32 sq2, int flag)
{
	return static_cast<uint16>(sq1 | (sq2 << 6) | (flag << 12));
}
inline uint16 to_sq1(uint16 move)
{
	return move & 0x3fU;
}
inline uint16 to_sq2(uint16 move)
{
	return (move & 0xfc0U) >> 6;
}
inline uint8 to_flag(uint16 move)
{
	return static_cast<uint8>(move >> 12);
}

//// global enums

enum square_e
{
	h1, g1, f1, e1, d1, c1, b1, a1,
	h2, g2, f2, e2, d2, c2, b2, a2,
	h3, g3, f3, e3, d3, c3, b3, a3,
	h4, g4, f4, e4, d4, c4, b4, a4,
	h5, g5, f5, e5, d5, c5, b5, a5,
	h6, g6, f6, e6, d6, c6, b6, a6,
	h7, g7, f7, e7, d7, c7, b7, a7,
	h8, g8, f8, e8, d8, c8, b8, a8
};

enum turn_e
{
	white,
	black
};

enum slide_e
{
	rook,
	bishop
};

enum piece_e
{
	PAWNS,
	ROOKS,
	KNIGHTS,
	BISHOPS,
	QUEENS,
	KINGS,
	NONE
};

enum pawn_e
{
	ENPASSANT = 7,
};

enum castl_e
{
	SHORT_WHITE = 8,
	LONG_WHITE = 9,
	SHORT_BLACK = 10,
	LONG_BLACK = 11
};

enum promo_e
{
	PROMO_ROOK = 12,
	PROMO_KNIGHT = 13,
	PROMO_BISHOP = 14,
	PROMO_QUEEN = 15
};

enum side_e
{
	WHITE,
	BLACK,
	BOTH
};

enum gen_e
{
	QUIETS,
	CAPTURES,
	ALL
};

enum stage_e
{
	MG,
	EG
};

enum state_e
{
	ACTIVE,
	CHECKMATE,
	STALEMATE,
	ISDRAW
};

enum castlright_e
{
	SW,
	LW,
	SB,
	LB
};

enum score_e
{
	DRAW = 0,
	MAX = 9900,
	MATE = 10000,
	NDEF = 11000
};

enum hash_e
{
	EXACT = 1,
	UPPER = 2,
	LOWER = 3
};