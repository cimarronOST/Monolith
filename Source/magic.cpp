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


#include <vector>
#include <bit>

#include "main.h"
#include "types.h"
#include "misc.h"
#include "bit.h"
#include "magic.h"

#if defined(PEXT)
#include <immintrin.h>
#endif

namespace magic
{
	// one table is used for both bishops and rooks, ~860 KB

	namespace size
	{
		constexpr int bishop{ 5248 };
		constexpr int rook{ 102400 };
		constexpr int table{ bishop + rook };
	}

	static void init_mask(piece pc)
	{
		// generating corresponding masks for each square

		for (square sq{ H1 }; sq <= A8; sq += 1)
		{
			bit64 sq_bit{ bit::set(sq) };
			for (int dr{ pc ^ 1 }; dr < 8; dr += 2)
			{
				bit64 ray{ sq_bit };
				while (!(ray & shift::dr[dr].boarder))
				{
					slider[sq][pc].mask |= ray;
					ray = bit::shift(ray, shift::dr[dr].shift);
				}
			}
			slider[sq][pc].mask ^= sq_bit;
		}
	}

	static void init_blocker(piece pc, std::vector<bit64> &blocker)
	{
		// generating all possible blocker boards for each square

		struct split_mask { bool on; bit64 bit; };
		for (auto &sq : slider)
		{
			std::vector<split_mask> split{};
			bit64 mask{ sq[pc].mask };

			while (mask)
			{
				split.push_back(split_mask{ false, bit::set(bit::scan(mask)) });
				mask &= mask - 1;
			}

			verify((int)split.size() >= 5 && (int)split.size() <= 12);
			verify((int)split.size() == std::popcount(sq[pc].mask));

			sq[pc].shift  = 64 - int(split.size());
			sq[pc].offset = int(blocker.size());

			// permuting through all possible combinations

			int permutations{ 1 << split.size() };
			for (int p{}; p < permutations; ++p)
			{
				mask = 0ULL;
				for (uint32 b{}; b < split.size(); ++b)
				{
					if (p % (1 << b) == 0)
						split[b].on = !split[b].on;

					if (split[b].on)
						mask |= split[b].bit;
				}
				blocker.push_back(mask);
			}
		}
	}

	static void init_attack(piece pc, const std::vector<bit64> &blocker, std::vector<bit64> &attack)
	{
		// generating all possible attacks from each square, considering the blocker boards

		for (square sq{ H1 }; sq <= A8; sq += 1)
		{
			bit64 sq_bit{ bit::set(sq) };
			int permutations{ 1 << (64 - slider[sq][pc].shift) };

			for (int p{}; p < permutations; ++p)
			{
				bit64 new_attack{};
				for (int dr{ pc ^ 1 }; dr < 8; dr += 2)
				{
					bit64 ray{ sq_bit };
					bit64 stop{ shift::dr[dr].boarder | blocker[slider[sq][pc].offset + p] };

					while (!(ray & stop))
					{
						ray = bit::shift(ray, shift::dr[dr].shift);
						new_attack |= ray;
					}
				}
				attack.push_back(new_attack);
				verify((int)attack.size() - 1 == slider[sq][pc].offset + p);
			}
		}
	}

	static void init_magic(piece pc, const std::vector<bit64> &blocker, const std::vector<bit64> &attack)
	{
		// finding fitting magic numbers
		// idea from Tord Romstad:
		// https://www.chessprogramming.org/Looking_for_Magics

		rand_64xor rand_gen{};
		bool fail{};

		for (square i{ H1 }; i <= A8; i += 1)
		{
			auto &sq{ slider[i][pc] };
			rand_gen.new_seed(i);
			
			int permutations{ 1 << (64 - sq.shift) };
			verify(permutations >= (1 << 5) && permutations <= (1 << 12));
			
			do
			{
				do sq.magic = rand_gen.sparse64();
				while (std::popcount((sq.mask * sq.magic) & 0xff00000000000000) < 6);

				fail = false;
				std::vector<bit64> save_attack(permutations);

				for (int p{}; !fail && p < permutations; ++p)
				{
					int index{ int(blocker[sq.offset + p] * sq.magic >> sq.shift) };
					verify(index <= permutations);

					if (!save_attack[index])
						 save_attack[index] = attack[sq.offset + p];

					else if (save_attack[index] != attack[sq.offset + p])
					{
						fail = true;
						break;
					}
				}
			} while (fail);
		}
	}

	static void init_index(piece pc, std::vector<bit64> &blocker, std::vector<bit64> &attack_temp)
	{
		// using the created magic numbers to index the attack table
		// if the BMI2 instruction PEXT is enabled, using it instead for faster performance

		for (auto &sq : slider)
		{
			int permutations{ 1 << (64 - sq[pc].shift) };
			for (int p{}; p < permutations; ++p)
			{
#if defined(PEXT)
				uint64 index{ _pext_u64(blocker[sq[pc].offset + p], sq[pc].mask) };
#else
				uint64 index{ blocker[sq[pc].offset + p] * sq[pc].magic >> sq[pc].shift };
#endif
				attack_table[sq[pc].offset + (int)index] = attack_temp[sq[pc].offset + p];
			}
		}
	}
}

void magic::init_table()
{
	// indexing the attack tables for bishops and rooks
	// starting by initializing temporary attack-tables & blocker-tables

	attack_table.resize(size::table);

	std::vector<bit64> attack_temp{};
	attack_temp.reserve(size::table);

	std::vector<bit64> blocker{};
	blocker.reserve(size::table);

	// generating magic numbers & indexing their final attack tables

	for (piece pc : { BISHOP, ROOK })
	{
		init_mask(pc);
		init_blocker(pc, blocker);
		init_attack(pc, blocker, attack_temp);
		init_magic(pc, blocker, attack_temp);
		init_index(pc, blocker, attack_temp);
	}
}
