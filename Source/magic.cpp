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


#include "random.h"
#include "bit.h"
#include "magic.h"

#if defined(PEXT)
#include <immintrin.h>
#endif

std::array<std::array<magic::sq_entry, 64>, 2> magic::slider{};
std::vector<bit64> magic::attack_table{};

namespace magic
{
	// one table is used for both bishops and rooks, ~860 KB

	namespace size
	{
		constexpr int bishop{ 5248 };
		constexpr int rook{ 102400 };
		constexpr int table{ bishop + rook };
	}

	void init_mask(piece pc)
	{
		// generating corresponding masks for each square

		for (square sq{ h1 }; sq <= a8; sq += 1)
		{
			bit64 sq_bit{ bit::set(sq) };
			for (int dr{ pc ^ 1 }; dr < 8; dr += 2)
			{
				bit64 ray{ sq_bit };
				while (!(ray & shift::dr[dr].boarder))
				{
					slider[pc][sq].mask |= ray;
					ray = bit::shift(ray, shift::dr[dr].shift);
				}
			}
			slider[pc][sq].mask ^= sq_bit;
		}
	}

	void init_blocker(piece pc, std::vector<bit64> &blocker)
	{
		// generating all possible blocker boards for each square

		struct split_mask { bool on; bit64 bit; };
		for (auto &sq : slider[pc])
		{
			std::vector<split_mask> split{};
			bit64 mask{ sq.mask };

			while (mask)
			{
				split.push_back(split_mask{ false, bit::set(bit::scan(mask)) });
				mask &= mask - 1;
			}

			verify((int)split.size() >= 5 && (int)split.size() <= 12);
			verify((int)split.size() == bit::popcnt(sq.mask));

			sq.shift  = 64 - int(split.size());
			sq.offset = int(blocker.size());

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

	void init_attack(piece pc, const std::vector<bit64> &blocker, std::vector<bit64> &attack)
	{
		// generating all possible attacks from each square, considering the blocker boards

		for (square sq{ h1 }; sq <= a8; sq += 1)
		{
			bit64 sq_bit{ bit::set(sq) };
			int permutations{ 1 << (64 - slider[pc][sq].shift) };

			for (int p{}; p < permutations; ++p)
			{
				bit64 new_attack{};
				for (int dr{ pc ^ 1 }; dr < 8; dr += 2)
				{
					bit64 ray{ sq_bit };
					bit64 stop{ shift::dr[dr].boarder | blocker[slider[pc][sq].offset + p] };

					while (!(ray & stop))
					{
						ray = bit::shift(ray, shift::dr[dr].shift);
						new_attack |= ray;
					}
				}
				attack.push_back(new_attack);
				verify((int)attack.size() - 1 == slider[pc][sq].offset + p);
			}
		}
	}

	void init_magic(piece pc, const std::vector<bit64> &blocker, const std::vector<bit64> &attack)
	{
		// finding fitting magic numbers
		// idea from Tord Romstad:
		// https://www.chessprogramming.org/Looking_for_Magics

		rand_64xor rand_gen{};
		bool fail{};

		for (square i{ h1 }; i <= a8; i += 1)
		{
			auto &sq{ slider[pc][i] };
			rand_gen.new_seed(i);
			
			int permutations{ 1 << (64 - sq.shift) };
			verify(permutations >= (1 << 5) && permutations <= (1 << 12));
			
			do
			{
				do sq.magic = rand_gen.sparse64();
				while (bit::popcnt((sq.mask * sq.magic) & 0xff00000000000000) < 6);

				fail = false;
				std::vector<bit64> save_attack(permutations);

				for (int p{}; !fail && p < permutations; ++p)
				{
					int index{ static_cast<int>(blocker[sq.offset + p] * sq.magic >> sq.shift) };
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

	void init_index(piece pc, std::vector<bit64> &blocker, std::vector<bit64> &attack_temp)
	{
		// using the created magic numbers to index the attack table
		// if the BMI2 instruction PEXT is enabled, using it instead for faster performance

		for (auto &sq : slider[pc])
		{
			int permutations{ 1 << (64 - sq.shift) };
			for (int p{}; p < permutations; ++p)
			{
#if defined(PEXT)
				uint64 index{ _pext_u64(blocker[sq.offset + p], sq.mask) };
#else
				uint64 index{ blocker[sq.offset + p] * sq.magic >> sq.shift };
#endif
				attack_table[sq.offset + (int)index] = attack_temp[sq.offset + p];
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

	for (piece pc : { bishop, rook })
	{
		init_mask(pc);
		init_blocker(pc, blocker);
		init_attack(pc, blocker, attack_temp);
		init_magic(pc, blocker, attack_temp);
		init_index(pc, blocker, attack_temp);
	}
}
