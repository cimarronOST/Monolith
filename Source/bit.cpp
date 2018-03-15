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


#if defined(_MSC_VER)
#include <intrin.h>
#include <nmmintrin.h>
#endif

#include "bit.h"

// side-independently shifting a bitboard

uint64 bit::shift(uint64 b, int shift)
{
	return (b << shift) | (b >> (64 - shift));
}

void bit::real_shift(uint64 &b, int shift)
{
	b = bit::shift(b, shift);
}

// calculating the population count

#if defined(POPCNT) && defined(_MSC_VER)
	int bit::popcnt(uint64 b)
	{
		return static_cast<int>(_mm_popcnt_u64(b));
	}

#elif defined(POPCNT) && defined(__GNUC__)
	int bit::popcnt(uint64 b)
	{
		return __builtin_popcountll(b);
	}

#else
	int bit::popcnt(uint64 b)
	{
		b = b - ((b >> 1) & 0x5555555555555555);
		b = (b & 0x3333333333333333) + ((b >> 2) & 0x3333333333333333);
		b = (b + (b >> 4)) & 0x0f0f0f0f0f0f0f0f;
		b = (b * 0x0101010101010101) >> 56;
		return static_cast<int>(b);
	}
#endif

// finding the least significant bit

#if defined(POPCNT) && defined(_MSC_VER)
	namespace
	{
		unsigned long least_bit, *this_bit{ &least_bit };
	}

	unsigned long bit::scan(uint64 b)
	{
		assert(b);
		_BitScanForward64(this_bit, b);
		return least_bit;
	}

#elif defined(POPCNT) && defined(__GNUC__)
	unsigned long bit::scan(uint64 b)
	{
		assert(b);
		return __builtin_ctzll(b);
	}

#else
	namespace
	{
		const int index[]
		{
			 0,  1,  2, 16,  3, 10, 50, 17,
			 7,  4, 11, 26, 60, 51, 41, 18,
			14,  8,  5, 58, 12, 34, 36, 27,
			61, 38, 55, 52, 46, 42, 29, 19,
			63, 15,  9, 49,  6, 25, 59, 40,
			13, 57, 33, 35, 37, 54, 45, 28,
			62, 48, 24, 39, 56, 32, 53, 44,
			47, 23, 31, 43, 22, 30, 21, 20,
		};
	}

	unsigned long bit::scan(uint64 b)
	{
		assert(b);
		return index[((b & (-1 * b)) * 0x02450fcbd59dc6d3) >> 58];
	}
#endif