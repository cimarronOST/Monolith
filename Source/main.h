/*
  Monolith 0.3  Copyright (C) 2017 Jonas Mayr

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

// debugging controls

#define POPCNT
#define NDEBUG
#define NTEST
#define NLOG

// libraries

#include <iostream>
#include <string>
#include <cassert>

// debugging controls

#if defined(TEST)
#define ASSERT assert
#else
#define ASSERT(x) ((void)0)
#endif

#if defined(LOG)
#define log sync_log
#else
#define log std
#endif

// global enumerators

enum SQUARE_INDEX
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

enum FILE_INDEX
{
	H, G, F, E, D, C, B, A
};

enum RANK_INDEX
{
	R1, R2, R3, R4, R5, R6, R7, R8
};

enum PIECE
{
	PAWNS = 0,
	KNIGHTS = 1,
	BISHOPS = 2,
	ROOKS = 3,
	QUEENS = 4,
	KINGS = 5,
	NONE = 7
};

enum PAWN_MOVE
{
	DOUBLEPUSH = 5,
	ENPASSANT = 6
};

enum CASTLING
{
	WHITE_SHORT = 8,
	BLACK_SHORT = 9,
	WHITE_LONG = 10,
	BLACK_LONG = 11
};

enum PROMO
{
	PROMO_KNIGHT = 12,
	PROMO_BISHOP = 13,
	PROMO_ROOK = 14,
	PROMO_QUEEN = 15
};

enum SLIDER
{
	ROOK,
	BISHOP
};

enum SIDE
{
	WHITE,
	BLACK,
	BOTH
};

enum GEN_MODE
{
	PSEUDO,
	LEGAL
};

enum GEN_STAGE
{
	CAPTURE,
	QUIET,
	TACTICAL,
	WINNING,
	LOOSING,
	HASH,
	ALL,
};

enum GAME_STAGE
{
	MG,
	EG
};

enum CASTLING_RIGHT
{
	WS,
	BS,
	WL,
	BL
};

enum SCORE
{
	NO_SCORE = 20000,
	MAX_SCORE = 10000,
	MATE_SCORE = 9000,
};

enum MOVE
{
	NO_MOVE
};

enum HASH
{
	EXACT = 1,
	UPPER = 2,
	LOWER = 3
};

// types

typedef unsigned long long uint64;
typedef unsigned int uint32;
typedef unsigned short uint16;
typedef signed short int16;
typedef unsigned char uint8;

struct move_detail
{
	int sq1;
	int sq2;
	int piece;
	int victim;
	int turn;
	uint8 flag;
};

// global constants

const std::string version{ "0.3" };

namespace lim
{
	const uint64 nodes{ ~0ULL };
	const uint64 movetime{ ~0ULL };

	const int depth{ 128 };

	const int period{ 1024 };
	const int movegen{ 256 };

	const int hash{ 4096 };
	const int min_cont{ -100 };
	const int max_cont{  100 };
}

const uint64 file[]
{
	0x0101010101010101,
	0x0202020202020202,
	0x0404040404040404,
	0x0808080808080808,
	0x1010101010101010,
	0x2020202020202020,
	0x4040404040404040,
	0x8080808080808080
};

const uint64 rank[]
{
	0xffULL,
	0xffULL << 8,
	0xffULL << 16,
	0xffULL << 24,
	0xffULL << 32,
	0xffULL << 40,
	0xffULL << 48,
	0xffULL << 56
};

namespace postfix
{
	const std::string promo[]{ "n", "b", "r", "q" };
}

// global functions

inline uint64 shift(uint64 bb, int shift)
{
	return (bb << shift) | (bb >> (64 - shift));
}

inline int to_sq1(uint32 move)
{
	return move & 0x3f;
}

inline int to_sq2(uint32 move)
{
	return (move >> 6) & 0x3f;
}

inline uint8 to_flag(uint32 move)
{
	return static_cast<uint8>((move >> 12) & 0xf);
}

inline int to_piece(uint32 move)
{
	return (move >> 16) & 0x7;
}

inline int to_victim(uint32 move)
{
	return (move >> 19) & 0x7;
}

inline int to_turn(uint32 move)
{
	assert(move >> 23 == 0);
	return move >> 22;
}

inline std::string to_promo(uint8 flag)
{
	assert(flag > 0 && flag < 16);
	return flag >= 12 ? postfix::promo[flag - 12] : "";
}

inline int to_idx(const std::string &sq)
{
	assert(sq.size() == 2);
	return 'h' - sq.front() + ((sq.back() - '1') << 3);
}

inline uint64 to_bb(const std::string sq)
{
	return 1ULL << to_idx(sq);
}

inline std::string to_str(const int sq)
{
	std::string str;
	str += 'h' - static_cast<char>(sq & 7);
	str += '1' + static_cast<char>(sq >> 3);

	return str;
}

inline uint32 encode(uint32 sq1, uint32 sq2, int flag, int piece, int victim, int turn)
{
	assert(turn == (turn & 1));
	assert(piece <= 5);
	assert(victim <= 4 || victim == 7);

	return sq1 | (sq2 << 6) | (flag << 12) | (piece << 16) | (victim << 19) | (turn << 22);
}

inline move_detail decode(uint32 move)
{
	return{ to_sq1(move), to_sq2(move), to_piece(move), to_victim(move), to_turn(move), to_flag(move) };
}

inline std::string algebraic(uint32 move)
{
	return to_str(to_sq1(move)) + to_str(to_sq2(move)) + to_promo(to_flag(move));
}
