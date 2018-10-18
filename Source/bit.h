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

#include "main.h"

// bitwise operations & constants
// supporting specific hardware instructions to speed up bitboard operations

namespace bit
{
	// bitboard tables concerning files

	constexpr uint64 file[]
	{
		0x0101010101010101ULL,
		0x0202020202020202ULL,
		0x0404040404040404ULL,
		0x0808080808080808ULL,
		0x1010101010101010ULL,
		0x2020202020202020ULL,
		0x4040404040404040ULL,
		0x8080808080808080ULL
	};

	constexpr uint64 file_west[2]{ file[A], file[H] };
	constexpr uint64 file_east[2]{ file[H], file[A] };

	// bitboard tables concerning ranks

	constexpr uint64 rank[]
	{
		0xffULL << 0,
		0xffULL << 8,
		0xffULL << 16,
		0xffULL << 24,
		0xffULL << 32,
		0xffULL << 40,
		0xffULL << 48,
		0xffULL << 56
	};

	constexpr uint64 rank_promo{ rank[R1] | rank[R8] };

	// bitboard tables concerning color

	constexpr uint64 white{ 0xaa55aa55aa55aa55 };
	constexpr uint64 black{ 0x55aa55aa55aa55aa };

	uint64 color(uint64 b);

	// performing bitboard operations

	uint64 shift(uint64 b, int shift);
	void real_shift(uint64 &b, int shift);

	int popcnt(uint64 b);
	unsigned long scan(uint64 b);

	uint32 byteswap32(uint32 b);
	uint64 byteswap64(uint64 b);
}

// directing the shift of bitboards

namespace shift
{
	constexpr int capture_west[]{  9, 55,  -9 };
	constexpr int capture_east[]{  7, 57,  -7 };
	constexpr int push[]        {  8, 56,  -8 };
	constexpr int push2x[]      { 16, 48, -16 };
}