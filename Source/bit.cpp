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


#include <bit>
#include <cstdlib>

#include "main.h"
#include "types.h"
#include "bit.h"

void bit::init_masks()
{
	// initializing all bitmasks

	for (int fl{ FILE_H }; fl <= FILE_A; ++fl)
	{
		// adjacent file map & adjacent en-passant squares

		fl_adjacent[fl] |= fl > FILE_H ? bit::file[fl - 1] : 0ULL;
		fl_adjacent[fl] |= fl < FILE_A ? bit::file[fl + 1] : 0ULL;

		ep_adjacent[WHITE][fl] = fl_adjacent[fl] & bit::rank[RANK_4];
		ep_adjacent[BLACK][fl] = fl_adjacent[fl] & bit::rank[RANK_5];
	}

	for (square sq{ H1 }; sq <= A8; sq += 1)
	{
		bit64 sq_bit{ bit::set(sq) };
		::file fl{ type::fl_of(sq) };
		::rank rk{ type::rk_of(sq) };

		// in-front, file-in-front & front-span

		in_front[WHITE][sq] = ~(sq_bit - 1) & ~bit::rank[type::rk_of(sq)];
		in_front[BLACK][sq] =  (sq_bit - 1) & ~bit::rank[type::rk_of(sq)];
		
		fl_in_front[WHITE][sq] = in_front[WHITE][sq] & bit::file[fl];
		fl_in_front[BLACK][sq] = in_front[BLACK][sq] & bit::file[fl];
		
		bit64 file_span{ bit::file[fl] | fl_adjacent[fl] };
		front_span[WHITE][sq] = file_span & in_front[WHITE][sq];
		front_span[BLACK][sq] = file_span & in_front[BLACK][sq];

		fork_in_front[WHITE][sq] = front_span[WHITE][sq] & ~bit::file[fl];
		fork_in_front[BLACK][sq] = front_span[BLACK][sq] & ~bit::file[fl];

		// knight attack map

		for (auto& dr : shift::knight_dr)
			pc_attack[KNIGHT][sq] |= sq_bit & dr.boarder ? 0ULL : shift(sq_bit, dr.shift);

		// bishop attack map (using only odd directions)

		for (int dr{ 1 }; dr < 8; dr += 2)
		{
			bit64 b_ray{ sq_bit };
			while (!(b_ray & shift::dr[dr].boarder))
			{
				b_ray = shift(b_ray, shift::dr[dr].shift);
				pc_attack[BISHOP][sq] |= b_ray;
			}
		}

		// rook attack map (using only even directions)

		for (int dr{}; dr < 8; dr += 2)
		{
			bit64 r_ray{ sq_bit };
			while (!(r_ray & shift::dr[dr].boarder))
			{
				r_ray = shift(r_ray, shift::dr[dr].shift);
				pc_attack[ROOK][sq] |= r_ray;
			}
		}

		// queen attack map

		pc_attack[QUEEN][sq] = pc_attack[ROOK][sq] | pc_attack[BISHOP][sq];

		// king attack & king zone map

		for (auto& dr : shift::dr)
			pc_attack[KING][sq] |= sq_bit & dr.boarder ? 0ULL : shift(sq_bit, dr.shift);

		for (auto cl : { WHITE, BLACK })
		{
			king_zone[cl][sq]  = pc_attack[KING][sq];
			king_zone[cl][sq] |= shift(pc_attack[KING][sq], shift::push1x[cl]);
			if (sq % 8 == 0)
				king_zone[cl][sq] |= pc_attack[KING][sq] << 1;
			if (sq % 8 == 7)
				king_zone[cl][sq] |= pc_attack[KING][sq] >> 1;
		}

		// maps to define pawn attacks & connected pawns

		pawn_attack[WHITE][sq] = pc_attack[KING][sq] & pc_attack[BISHOP][sq] & in_front[WHITE][sq];
		pawn_attack[BLACK][sq] = pc_attack[KING][sq] & pc_attack[BISHOP][sq] & in_front[BLACK][sq];

		connected[WHITE][sq] = (fl_adjacent[fl] & bit::rank[rk]) | pawn_attack[BLACK][sq];
		connected[BLACK][sq] = (fl_adjacent[fl] & bit::rank[rk]) | pawn_attack[WHITE][sq];

		for (square sq2{ H1 }; sq2 <= A8; sq2 += 1)
		{
			// map of bits between two bits

			square sq_max{ std::max(sq, sq2) };
			square sq_min{ std::min(sq, sq2) };
			between[sq][sq2] = (sq_max == A8 ? max : (set(sq_max + 1) - 1)) & ~(set(sq_min) - 1);

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

	verify(std::popcount(bb) == 1);
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

	return (square)std::countr_zero(bb);
}

template uint16 bit::byteswap<uint16>(uint16);
template uint32 bit::byteswap<uint32>(uint32);
template uint64 bit::byteswap<uint64>(uint64);
template<typename uint>
uint bit::byteswap(uint bb)
{
	// classical Intel Compiler doesn't fully support C++23 but still produces the fastest binaries

#if defined(_WIN32) && defined(__INTEL_LLVM_COMPILER) && !defined(SYCL_LANGUAGE_VERSION)
	if constexpr (sizeof(uint) == 2)
		return _byteswap_ushort(bb);
	if constexpr (sizeof(uint) == 4)
		return _byteswap_ulong(bb);
	if constexpr (sizeof(uint) == 8)
		return _byteswap_uint64(bb);
#else
	return std::byteswap(bb);
#endif
}

// managing endianness

template uint16 bit::l_endian<uint16>(uint16);
template uint32 bit::l_endian<uint32>(uint32);
template<typename uint>
uint bit::l_endian(uint bb)
{
	return std::endian::native == std::endian::little ? bb : byteswap(bb);
}

template uint16 bit::b_endian<uint16>(uint16);
template uint32 bit::b_endian<uint32>(uint32);
template uint64 bit::b_endian<uint64>(uint64);
template<typename uint>
uint bit::b_endian(uint bb)
{
	return std::endian::native == std::endian::big ? bb : byteswap(bb);
}

template uint16 bit::read_l_endian<uint16>(void*);
template uint32 bit::read_l_endian<uint32>(void*);
template<typename uint>
uint bit::read_l_endian(void* bb)
{
	return l_endian<uint>(*(uint*)bb);
}
