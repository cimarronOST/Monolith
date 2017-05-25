/*
  Monolith 0.2  Copyright (C) 2017 Jonas Mayr

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

// compile controls

#define NSSE4
#define NDEBUG
#define LOG_OFF

// global includes

#include <iostream>
#include <string>
#include <cassert>

// defines

#ifdef LOG_ON
#define log sync_log
#else
#define log std
#endif

// namespaces

using std::endl;
using std::string;

// global strings

const string version{ "0.2" };
const string startpos{ "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 0" };

// typedefs

typedef unsigned long long uint64;
typedef unsigned int uint32;
typedef unsigned short uint16;
typedef unsigned char uint8;
typedef signed short int16;

// global constants

const uint8 castl_right[]{ 0x1, 0x4, 0x10, 0x40 };

namespace lim
{
	const int movetime{ 0x7fffffff };
	const int depth{ 64 };
	const int period{ 1024 };
	const int movegen{ 256 };
	const int hash{ 1024 };
}

const uint64 file[]
{
	0x0101010101010101, 0x0202020202020202, 0x0404040404040404, 0x0808080808080808,
	0x1010101010101010, 0x2020202020202020, 0x4040404040404040, 0x8080808080808080
};

const uint64 rank[]
{
	0x00000000000000ff, 0x000000000000ff00, 0x0000000000ff0000, 0x00000000ff000000,
	0x000000ff00000000, 0x0000ff0000000000, 0x00ff000000000000, 0xff00000000000000
};

// global functions

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

// global enums

enum square_e
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

enum file_e
{
	H, G, F, E, D, C, B, A
};

enum rank_e
{
	R1, R2, R3, R4, R5, R6, R7, R8
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

enum slide_e
{
	ROOK,
	BISHOP
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
