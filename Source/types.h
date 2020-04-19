/*
  Monolith 2 Copyright (C) 2017-2020 Jonas Mayr
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

#include "main.h"

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
	h1, g1, f1, e1, d1, c1, b1, a1,
	h2, g2, f2, e2, d2, c2, b2, a2,
	h3, g3, f3, e3, d3, c3, b3, a3,
	h4, g4, f4, e4, d4, c4, b4, a4,
	h5, g5, f5, e5, d5, c5, b5, a5,
	h6, g6, f6, e6, d6, c6, b6, a6,
	h7, g7, f7, e7, d7, c7, b7, a7,
	h8, g8, f8, e8, d8, c8, b8, a8,
	prohibited
};

enum file : int { file_h, file_g, file_f, file_e, file_d, file_c, file_b, file_a };
enum rank : int { rank_1, rank_2, rank_3, rank_4, rank_5, rank_6, rank_7, rank_8 };

enum piece : int16
{
	pawn,
	knight,
	bishop,
	rook,
	queen,
	king,
	no_piece
};

enum color     : int { white, black, both };
enum gamestage : int { mg, eg };

enum flag : int
{
	castle_east,
	castle_west,
	enpassant,
	no_flag,
	promo_knight,
	promo_bishop,
	promo_rook,
	promo_queen
};

enum score : int
{
	none         = -32100,
	blessed_loss = -2,
	draw         =  0,
	cursed_win   =  2,
	mate         =  32000,
	longest_mate =  31780,
	tb_win       =  16000,
	tb_loss      = -16000
};

// using scoped enumerators wherever possible

enum class direction : int
{
	south = -8,
	north =  8,
	east  = -1,
	west  =  1
};

enum class bound  : int { none, exact, upper, lower };
enum class mode   : int { legal, pseudolegal };

enum class genstage : int
{
	hash,
	winning,
	killer,
	quiet,
	loosing,
	deferred,
	capture,
	tactical,
	check,
	evasion
};

enum class stage : int
{
	quiet,
	quiet_promo_all,
	quiet_promo_queen,
	capture,
	capture_promo_all,
	capture_promo_queen,
	enpassant,
	promo_all,
	promo_queen
};

enum class exception : int { stop_search };

// operator overloads

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
	constexpr int threads{ 128 };
	constexpr int syzygy_pieces{ 7 };

	constexpr milliseconds movetime{ milliseconds::max() };
	constexpr milliseconds overhead{ 5000 };

	constexpr std::size_t hash{ 65536 };
	constexpr std::size_t moves{ 218 };
	constexpr std::size_t multipv{ moves };

	constexpr depth dt{ 110 };
	constexpr depth dtz{ 1048 };

	static_assert(score::longest_mate + 2 * lim::dt == score::mate);
}

namespace type
{
	// asserting type correctness for all unscoped enumerator types

	constexpr bool sq(square sq) { return sq >= h1     && sq <= a8;      }
	constexpr bool fl(file fl)   { return fl >= file_h && fl <= file_a;  }
	constexpr bool rk(rank rk)   { return rk >= rank_1 && rk <= rank_8;  }
	constexpr bool cl(color cl)  { return cl == white  || cl == black;   }
	constexpr bool pc(piece pc)  { return pc >= pawn   && pc <= king;    }
	constexpr bool fl(flag fl)   { return fl >= 0      && fl <= 0b111;   }
	constexpr bool sc(score sc)  { return sc >= -mate  && sc <= mate;    }
	constexpr bool dt(depth dt)  { return dt >= 0      && dt <= lim::dt; }

	// basic global type functions

	file fl_of(square sq);
	rank rk_of(square sq);
	rank rel_rk_of(rank rk, color cl);

	square sq_of(rank rk, file fl);
	square sq_of(std::string sq);
	std::string sq_of(square sq);

	square sq_flip(square sq);
	int sq_distance(square sq1, square sq2);
}
