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


#if defined(__INTEL_COMPILER)
#include <nmmintrin.h>
#endif

#if defined(_MSC_VER)
#include <nmmintrin.h>
#include <stdlib.h>
#include <intrin.h>
#endif

#include "bit.h"

std::array<bit64, 8> bit::fl_adjacent{};
std::array<std::array<bit64,  8>,  2> bit::ep_adjacent{};
std::array<std::array<bit64, 64>,  2> bit::in_front{};
std::array<std::array<bit64, 64>,  2> bit::fl_in_front{};
std::array<std::array<bit64, 64>,  2> bit::front_span{};
std::array<std::array<bit64, 64>, 64> bit::between{};
std::array<std::array<bit64, 64>, 64> bit::ray{};
std::array<std::array<bit64, 64>,  6> bit::pc_attack{};
std::array<std::array<bit64, 64>,  2> bit::pawn_attack{};
std::array<std::array<bit64, 64>,  2> bit::king_zone{};
std::array<std::array<bit64, 64>,  2> bit::connected{};

void bit::init_masks()
{
	// initializing all bitmasks

	for (int fl{ file_h }; fl <= file_a; ++fl)
	{
		// adjacent file map & adjacent en-passant squares

		fl_adjacent[fl] |= fl > file_h ? bit::file[fl - 1] : 0ULL;
		fl_adjacent[fl] |= fl < file_a ? bit::file[fl + 1] : 0ULL;

		ep_adjacent[white][fl] = fl_adjacent[fl] & bit::rank[rank_4];
		ep_adjacent[black][fl] = fl_adjacent[fl] & bit::rank[rank_5];
	}

	for (square sq{ h1 }; sq <= a8; sq += 1)
	{
		bit64 sq_bit{ bit::set(sq) };
		::file fl{ type::fl_of(sq) };
		::rank rk{ type::rk_of(sq) };

		// in-front, file-in-front & front-span

		in_front[white][sq] = ~(sq_bit - 1) & ~bit::rank[type::rk_of(sq)];
		in_front[black][sq] =  (sq_bit - 1) & ~bit::rank[type::rk_of(sq)];
		
		fl_in_front[white][sq] = in_front[white][sq] & bit::file[fl];
		fl_in_front[black][sq] = in_front[black][sq] & bit::file[fl];
		
		bit64 file_span{ bit::file[fl] | fl_adjacent[fl] };
		front_span[white][sq] = file_span & in_front[white][sq];
		front_span[black][sq] = file_span & in_front[black][sq];

		// knight attack map

		for (auto& dr : shift::knight_dr)
			pc_attack[knight][sq] |= sq_bit & dr.boarder ? 0ULL : shift(sq_bit, dr.shift);

		// bishop attack map (using only odd directions)

		for (int dr{ 1 }; dr < 8; dr += 2)
		{
			bit64 ray{ sq_bit };
			while (!(ray & shift::dr[dr].boarder))
			{
				ray = shift(ray, shift::dr[dr].shift);
				pc_attack[bishop][sq] |= ray;
			}
		}

		// rook attack map (using only even directions)

		for (int dr{}; dr < 8; dr += 2)
		{
			bit64 ray{ sq_bit };
			while (!(ray & shift::dr[dr].boarder))
			{
				ray = shift(ray, shift::dr[dr].shift);
				pc_attack[rook][sq] |= ray;
			}
		}

		// queen attack map

		pc_attack[queen][sq] = pc_attack[rook][sq] | pc_attack[bishop][sq];

		// king attack & king zone map

		for (auto& dr : shift::dr)
			pc_attack[king][sq] |= sq_bit & dr.boarder ? 0ULL : shift(sq_bit, dr.shift);

		king_zone[white][sq] = pc_attack[king][sq] | shift(pc_attack[king][sq], shift::push1x[white]);
		king_zone[black][sq] = pc_attack[king][sq] | shift(pc_attack[king][sq], shift::push1x[black]);

		// maps to define pawn attacks & connected pawns

		pawn_attack[white][sq] = pc_attack[king][sq] & pc_attack[bishop][sq] & in_front[white][sq];
		pawn_attack[black][sq] = pc_attack[king][sq] & pc_attack[bishop][sq] & in_front[black][sq];

		connected[white][sq] = (fl_adjacent[fl] & bit::rank[rk]) | pawn_attack[black][sq];
		connected[black][sq] = (fl_adjacent[fl] & bit::rank[rk]) | pawn_attack[white][sq];

		for (square sq2{ h1 }; sq2 <= a8; sq2 += 1)
		{
			// map of bits between two bits

			square sq_max{ std::max(sq, sq2) };
			square sq_min{ std::min(sq, sq2) };
			between[sq][sq2] = (sq_max == a8 ? max : (set(sq_max + 1) - 1)) & ~(set(sq_min) - 1);

			// map of ray between two bits

			bit64 sq2_bit{ set(sq2) };
			bool success{ false };
			for (auto &dr : shift::dr)
			{
				if (success)
					break;
				bit64 new_ray{ sq_bit };
				while (!(new_ray & dr.boarder))
				{
					new_ray |= shift(new_ray, dr.shift);
					if (new_ray & sq2_bit)
					{
						success = true;
						ray[sq][sq2] = new_ray;
						break;
					}
				}
			}
		}
	}
}

bit64 bit::shift(bit64 bb, int shift)
{
	// shifting a bitboard without the use of conditionals

	return (bb << shift) | (bb >> (64 - shift));
}

bit64 bit::color(bit64 bb)
{
	// returning all squares of the same color as the given bitboard

	verify(popcnt(bb) == 1);
	return (bb & sq_white) ? sq_white : sq_black;
}

bit64 bit::set(square sq)
{
	// creating a bitboard out of a given square

	verify(type::sq(sq));
	return 1ULL << sq;
}

square bit::scan(bit64 bb)
{
	// finding the least significant bit
	// using fast processor instructions if possible

	verify(bb);

#if defined(POPCNT) && defined(__GNUC__)
	return square(__builtin_ctzll(bb));

#elif defined(POPCNT) && (defined(_MSC_VER) || defined(__INTEL_COMPILER))
	unsigned long lsb{};
	void(_BitScanForward64(&lsb, bb));
	return square(lsb);

#else
	// using slower calculations instead

	static constexpr std::array<square, 64> sq
	{ {
		h1, g1, f1, h3, e1, f2, f7, g3,
		a1, d1, e2, f4, d8, e7, g6, f3,
		b2, h2, c1, f8, d2, f5, d5, e4,
		c8, b5, a7, d7, b6, f6, c4, e3,
		a8, a2, g2, g7, b1, g4, e8, h6,
		c2, g8, g5, e5, c5, b7, c6, d4,
		b8, h7, h4, a5, h8, h5, c7, d6,
		a6, a3, a4, e6, b3, b4, c3, d3,
	} };
	return sq[((bb & -int64(bb)) * 0x02450fcbd59dc6d3ULL) >> 58];

#endif
}

int bit::popcnt(bit64 bb)
{
	// calculating the population count
	// using fast processor instructions if possible

#if defined(POPCNT) && defined(__GNUC__)
	return __builtin_popcountll(bb);

#elif defined(POPCNT) && (defined(_MSC_VER) || defined(__INTEL_COMPILER))
	return int(_mm_popcnt_u64(bb));

#else
	// using slower calculations instead

	bb = bb - ((bb >> 1) & 0x5555555555555555ULL);
	bb = (bb & 0x3333333333333333ULL) + ((bb >> 2) & 0x3333333333333333ULL);
	bb = (bb + (bb >> 4)) & 0x0f0f0f0f0f0f0f0fULL;
	return int((bb * 0x0101010101010101ULL) >> 56);

#endif
}

template uint16 bit::byteswap<uint16>(uint16);
template uint32 bit::byteswap<uint32>(uint32);
template uint64 bit::byteswap<uint64>(uint64);

template<typename uint>
uint bit::byteswap(uint bb)
{
	// reversing the order of bytes

	static constexpr std::size_t size{ sizeof(uint) };
	static_assert(size == 2 || size == 4 || size == 8);

#if defined(__INTEL_COMPILER)
	if constexpr (size == 2) return (bb >> 8) | (bb << 8);
	if constexpr (size == 4) return _bswap(bb);
	if constexpr (size == 8) return _bswap64(bb);

#elif defined(_MSC_VER)
	if constexpr (size == 2) return _byteswap_ushort(bb);
	if constexpr (size == 4) return _byteswap_ulong(bb);
	if constexpr (size == 8) return _byteswap_uint64(bb);

#elif defined(__GNUC__)
	if constexpr (size == 2) return __builtin_bswap16(bb);
	if constexpr (size == 4) return __builtin_bswap32(bb);
	if constexpr (size == 8) return __builtin_bswap64(bb);

#else
	uint bb_swap{};
	int8 *ptr_1{ (int8*)&bb };
	int8 *ptr_2{ (int8*)&bb_swap };

	for (uint64 i{}; i < sizeof(uint); ++i)
		ptr_2[i] = ptr_1[sizeof(uint) - 1 - i];
	return uint(bb_swap);

#endif
	verify(false);
	return 0;
}

// managing endianess

namespace
{
	bool little_endian()
	{
		union { uint16 s; uint8 c[2]; } constexpr num{ 1 };
		return num.c[0] == 1;
	}
}

template uint16 bit::le<uint16>(uint16);
template uint32 bit::le<uint32>(uint32);

template<typename uint>
uint bit::le(uint bb)
{
	return little_endian() ? bb : byteswap<uint>(bb);
}

template uint16 bit::be<uint16>(uint16);
template uint32 bit::be<uint32>(uint32);
template uint64 bit::be<uint64>(uint64);

template<typename uint>
uint bit::be(uint bb)
{
	return little_endian() ? byteswap<uint>(bb) : bb;
}

template uint16 bit::read_le<uint16>(void*);
template uint32 bit::read_le<uint32>(void*);

template<typename uint>
uint bit::read_le(void* bb)
{
	return le<uint>(*(uint*)bb);
}
