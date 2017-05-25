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


#ifdef _MSC_VER
#include <intrin.h>
#include <nmmintrin.h>
#endif

#include "bitboard.h"

namespace
{
	unsigned long LSB, *ptr{ &LSB };
}

unsigned long bb::lsb()
{
	return LSB;
}

#if defined(SSE4) && defined(_MSC_VER)
	int bb::popcnt(uint64 board)
	{
		return static_cast<int>(_mm_popcnt_u64(board));
	}
#elif defined(SSE4) && defined(__GNUC__)
	int bb::popcnt(uint64 board)
	{
		return __builtin_popcountll(board);
	}
#else
	int bb::popcnt(uint64 board)
	{
		board = board - ((board >> 1) & 0x5555555555555555);
		board = (board & 0x3333333333333333) + ((board >> 2) & 0x3333333333333333);
		board = (board + (board >> 4)) & 0x0f0f0f0f0f0f0f0f;
		board = (board * 0x0101010101010101) >> 56;
		return static_cast<int>(board);
	}
#endif

#if defined(SSE4) && defined(_MSC_VER)
	void bb::bitscan(uint64 board)
	{
		assert(board != 0);
		_BitScanForward64(ptr, board);
	}
#elif defined(SSE4) && defined(__GNUC__)
	void bb::bitscan(uint64 board)
	{
		assert(board != 0);
		LSB = __builtin_ctzll(board);
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
	void bb::bitscan(uint64 board)
	{
		assert(board != 0);
		LSB = index[((board & (-1 * board)) * 0x02450fcbd59dc6d3) >> 58];
	}
#endif
