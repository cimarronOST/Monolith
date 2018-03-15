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

#include "main.h"

// bitwise operations & constants
// supporting specific hardware instructions to speed up bitboard operations

namespace bit
{
	const uint64 file[]
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

	const uint64 rank[]
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

	uint64 shift(uint64 b, int shift);
	void real_shift(uint64 &b, int shift);

	int popcnt(uint64 b);
	unsigned long scan(uint64 b);
}