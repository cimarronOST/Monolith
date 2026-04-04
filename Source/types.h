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

#include <chrono>
#include <array>
#include <iostream>
#include <limits>
#include <string>

// basic type definitions

using  int8 =   signed char;
using uint8 = unsigned char;

using  int16 =   signed short;
using uint16 = unsigned short;

using  int32 =   signed int;
using uint32 = unsigned int;

using  int64 =   signed long long;
using uint64 = unsigned long long;

static_assert(sizeof(int8)  == 1);
static_assert(sizeof(int16) == 2);
static_assert(sizeof(int32) == 4);
static_assert(sizeof(int64) == 8);

using bit64 = uint64;
using key64 = uint64;
using key32 = uint32;

// additional chess specific type definitions

using depth = int;
using piece_list = std::array<std::array<int, 6>, 2>;
class move;

enum square : int
{
	H1, G1, F1, E1, D1, C1, B1, A1,
	H2, G2, F2, E2, D2, C2, B2, A2,
	H3, G3, F3, E3, D3, C3, B3, A3,
	H4, G4, F4, E4, D4, C4, B4, A4,
	H5, G5, F5, E5, D5, C5, B5, A5,
	H6, G6, F6, E6, D6, C6, B6, A6,
	H7, G7, F7, E7, D7, C7, B7, A7,
	H8, G8, F8, E8, D8, C8, B8, A8,
	NO_SQUARE
};

enum file : int { FILE_H, FILE_G, FILE_F, FILE_E, FILE_D, FILE_C, FILE_B, FILE_A };
enum rank : int { RANK_1, RANK_2, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7, RANK_8 };

enum piece : int16
{
	PAWN,
	KNIGHT,
	BISHOP,
	ROOK,
	QUEEN,
	KING,
	NO_PIECE
};

enum color : int { WHITE, BLACK, BOTH };

enum flag : int
{
	CASTLE_EAST,
	CASTLE_WEST,
	ENPASSANT,
	NO_FLAG,
	PROMO_KNIGHT,
	PROMO_BISHOP,
	PROMO_ROOK,
	PROMO_QUEEN
};

enum score : int
{
	NONE         = -32100,
	BLESSED_LOSS = -2,
	DRAW         =  0,
	CURSED_WIN   =  2,
	MATE         =  32000,
	LONGEST_MATE =  31780,
	TB_WIN       =  16000,
	TB_LOSS      = -16000
};

// using scoped enumerators wherever possible

enum class direction : int
{
	SOUTH = -8,
	NORTH =  8,
	EAST  = -1,
	WEST  =  1
};

enum class bound  : int { NONE, EXACT, UPPER, LOWER };
enum class mode   : int { LEGAL, PSEUDOLEGAL };

enum class genstage : int
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

enum class stage : int
{
	QUIET,
	QUIET_PROMO_ALL,
	QUIET_PROMO_QUEEN,
	CAPTURE,
	CAPTURE_PROMO_ALL,
	CAPTURE_PROMO_QUEEN,
	ENPASSANT,
	PROMO_ALL,
	PROMO_QUEEN
};

enum class exception : int { STOP_SEARCH };

// operator overloads

using std::chrono::milliseconds;

constexpr score  operator + (score  sc1, score sc2) { return score(int(sc1) + int(sc2)); }
constexpr score  operator - (score  sc1, score sc2) { return score(int(sc1) - int(sc2)); }
constexpr score  operator * (score  sc, int mul) { return score(int(sc) * mul); }
constexpr score  operator / (score  sc, int div) { return score(int(sc) / div); }
constexpr score  operator - (score  sc) { return score(-int(sc)); }
constexpr void   operator +=(score& sc, int add) { sc = sc + score(add); }
constexpr void   operator -=(score& sc, int sub) { sc = sc - score(sub); }

constexpr square operator + (square  sq, int add) { return square(int(sq) + add); }
constexpr square operator - (square  sq, int sub) { return square(int(sq) - sub); }
constexpr void   operator +=(square& sq, int add) { sq = sq + add; }

constexpr piece  operator + (piece  pc, int add) { return piece(int(pc) + add); }
constexpr color  operator ^ (color  cl, int bit) { return color(int(cl) ^ bit); }
constexpr void   operator ^=(color& cl, int bit) { cl = cl ^ bit; }
constexpr flag   operator - (flag   fl, int sub) { return flag(int(fl) - sub); }

inline std::ostream& operator <<(std::ostream& os, const score& sc) { os << int(sc); return os; }
inline std::ostream& operator <<(std::ostream& os, const milliseconds& time) { os << time.count(); return os; }
inline std::istream& operator >>(std::istream& is, milliseconds& time) { int t{}; is >> t; time = milliseconds(t); return is; }

// limiting parameters

namespace lim
{
	constexpr int64 nodes{ std::numeric_limits<int64>::max() };
	constexpr int threads{ 512 };
	constexpr int syzygy_pieces{ 7 };

	constexpr milliseconds movetime{ milliseconds::max() };
	constexpr milliseconds overhead{ 5000 };

	constexpr std::size_t hash{ 262144 };
	constexpr std::size_t moves{ 218 };
	constexpr std::size_t multipv{ moves };

	constexpr depth dt{ 110 };
	constexpr depth dtz{ 1048 };

    static_assert(LONGEST_MATE + 2 * lim::dt == MATE);
}


namespace type
{
	// asserting type correctness for all unscoped enumerator types

    constexpr bool sq(square sq) { return sq >= H1     && sq <= A8;      }
    constexpr bool fl(file fl)   { return fl >= FILE_H && fl <= FILE_A;  }
    constexpr bool rk(rank rk)   { return rk >= RANK_1 && rk <= RANK_8;  }
    constexpr bool cl(color cl)  { return cl == WHITE  || cl == BLACK;   }
    constexpr bool pc(piece pc)  { return pc >= PAWN   && pc <= KING;    }
    constexpr bool fl(flag fl)   { return fl >= 0      && fl <= 0b111;   }
    constexpr bool sc(score sc)  { return sc >= -MATE  && sc <= MATE;    }
	constexpr bool dt(depth dt)  { return dt >= 0      && dt <= lim::dt; }

	// basic global type functions

	file fl_of(square sq);
	rank rk_of(square sq);
	rank rk_of(rank rk, color cl);

	square sq_of(rank rk, file fl);
	square sq_of(std::string sq);
	std::string sq_of(square sq);

	square sq_flip(square sq);
	int    sq_distance(square sq1, square sq2);
}
