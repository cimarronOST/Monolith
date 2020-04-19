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

#include <limits>

#include "types.h"
#include "main.h"

// manipulating bitboards

namespace bit
{
	// bitmasks that have to be initialized

	extern std::array<bit64, 8> fl_adjacent;
	extern std::array<std::array<bit64,  8>,  2> ep_adjacent;
	extern std::array<std::array<bit64, 64>,  2> in_front;
	extern std::array<std::array<bit64, 64>,  2> fl_in_front;
	extern std::array<std::array<bit64, 64>,  2> front_span;
	extern std::array<std::array<bit64, 64>, 64> between;
	extern std::array<std::array<bit64, 64>, 64> ray;
	extern std::array<std::array<bit64, 64>,  6> pc_attack;
	extern std::array<std::array<bit64, 64>,  2> pawn_attack;
	extern std::array<std::array<bit64, 64>,  2> king_zone;
	extern std::array<std::array<bit64, 64>,  2> connected;

	void init_masks();

	// bit manipulation functions

	bit64 shift(bit64 bb, int shift);
	bit64 color(bit64 bb);
	bit64 set(square sq);

	square scan(bit64 bb);
	int popcnt(bit64 bb);

	template<typename uint> uint byteswap(uint bb);

	// managing little & big endianess

	template<typename uint> uint le(uint bb);
	template<typename uint> uint be(uint bb);
	template<typename uint> uint read_le(void* bb);

	// pre-calculated bitmasks

	constexpr std::array<bit64, 8> file
	{ {
		0x0101010101010101ULL,
		0x0202020202020202ULL,
		0x0404040404040404ULL,
		0x0808080808080808ULL,
		0x1010101010101010ULL,
		0x2020202020202020ULL,
		0x4040404040404040ULL,
		0x8080808080808080ULL
	} };

	constexpr std::array<bit64, 8> rank
	{ {
		0xffULL << 0,
		0xffULL << 8,
		0xffULL << 16,
		0xffULL << 24,
		0xffULL << 32,
		0xffULL << 40,
		0xffULL << 48,
		0xffULL << 56
	} };

	constexpr bit64 sq_white{ 0xaa55aa55aa55aa55ULL };
	constexpr bit64 sq_black{ 0x55aa55aa55aa55aaULL };
	constexpr bit64 half_east{ 0x0f0f0f0f0f0f0f0fULL };
	constexpr bit64 half_west{ 0xf0f0f0f0f0f0f0f0ULL };
	constexpr bit64 rank_promo{ rank[rank_1] | rank[rank_8] };
	constexpr bit64 max{ std::numeric_limits<bit64>::max() };

	constexpr std::array<bit64, 2> outpost_zone{ { 0x3c3c3c000000ULL, 0x3c3c3c0000ULL } };
	constexpr std::array<bit64, 2> board_half{ { 0xffffffffULL, 0xffffffff00000000ULL } };
	constexpr std::array<bit64, 2> no_west_boarder{ { ~bit::file[file_a], ~bit::file[file_h] } };
	constexpr std::array<bit64, 2> no_east_boarder{ { ~bit::file[file_h], ~bit::file[file_a] } };
	constexpr std::array<bit64, 2> rank_push2x{ { rank[rank_3], rank[rank_6] } };
}

// defining piece movements and their restrictions for all non-sliding pieces

namespace shift
{
	struct direction
	{
		int shift;
		bit64 boarder;
	};

	// pawn shifts

	constexpr std::array<int, 2> capture_west{ {  9, 55 } };
	constexpr std::array<int, 2> capture_east{ {  7, 57 } };
	constexpr std::array<int, 2> push1x{       {  8, 56 } };
	constexpr std::array<int, 2> push2x{       { 16, 48 } };

	// king shifts

	constexpr std::array<direction, 8> dr
	{ {
		{  8, 0xff00000000000000ULL },
	    {  7, 0xff01010101010101ULL },
	    { 63, 0x0101010101010101ULL },
	    { 55, 0x01010101010101ffULL },
	    { 56, 0x00000000000000ffULL },
	    { 57, 0x80808080808080ffULL },
	    {  1, 0x8080808080808080ULL },
	    {  9, 0xff80808080808080ULL }
	} };

	// knight shifts

	constexpr std::array<direction, 8> knight_dr
	{ {
		{ 15, 0xffff010101010101ULL },
	    {  6, 0xff03030303030303ULL },
		{ 54, 0x03030303030303ffULL },
	    { 47, 0x010101010101ffffULL },
		{ 49, 0x808080808080ffffULL },
	    { 58, 0xc0c0c0c0c0c0c0ffULL },
		{ 10, 0xffc0c0c0c0c0c0c0ULL },
	    { 17, 0xffff808080808080ULL }
	} };
}