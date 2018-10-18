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


#include "utilities.h"
#include "bit.h"
#include "magic.h"

magic::square_entry magic::slider[2][64]{};
std::vector<uint64> magic::attack_table;

namespace magic
{
	// one table is used for both rooks and bishops

	constexpr int table_size{ 102400 + 5248 };

	void init_mask(int sl)
	{
		// generating corresponding ray masks for each square

		assert(sl == ROOK || sl == BISHOP);

		for (int sq{ H1 }; sq <= A8; ++sq)
		{
			auto sq_bit{ 1ULL << sq };
			for (auto dir{ sl }; dir < 8; dir += 2)
			{
				auto flood{ sq_bit };
				while (!(flood & ray[dir].boarder))
				{
					slider[sl][sq].mask |= flood;
					bit::real_shift(flood, ray[dir].shift);
				}
			}
			slider[sl][sq].mask ^= sq_bit;
		}
	}

	void init_blocker(int sl, std::vector<uint64> &blocker)
	{
		// generating all possible blocker boards for each square

		assert(sl == ROOK || sl == BISHOP);
		assert(blocker.size() == 0 || blocker.size() == 102400);

		bool bit[12]{};

		for (int sq{ H1 }; sq <= A8; ++sq)
		{
			slider[sl][sq].offset = static_cast<int>(blocker.size());
			auto mask{ slider[sl][sq].mask };

			int bits_set{};
			uint64 split_mask[12]{};
			while (mask)
			{
				split_mask[bits_set++] = 1ULL << bit::scan(mask);
				mask &= mask - 1;
			}

			assert(bits_set <= 12);
			assert(bits_set >=  5);
			assert(bit::popcnt(slider[sl][sq].mask) == bits_set);

			slider[sl][sq].shift = 64 - bits_set;

			// permuting through all possible combinations

			auto permutations{ 1 << bits_set };
			for (int p{}; p < permutations; ++p)
			{
				mask = 0ULL;
				for (int b{}; b < bits_set; ++b)
				{
					if (p % (1 << b) == 0)
						bit[b] = !bit[b];

					if (bit[b])
						mask |= split_mask[b];
				}
				blocker.push_back(mask);
			}
		}
	}

	void init_move(int sl, std::vector<uint64> &blocker, std::vector<uint64> &attack)
	{
		// generating all possible attacks from each square, considering the blocker boards

		assert(sl == ROOK || sl == BISHOP);
		assert(attack.size() == 0 || attack.size() == 102400);

		for (int sq{ H1 }; sq <= A8; ++sq)
		{
			auto sq_bit{ 1ULL << sq };

			auto permutations{ 1 << (64 - slider[sl][sq].shift) };
			for (int p{}; p < permutations; ++p)
			{
				uint64 new_attack{};
				for (auto dir{ sl }; dir < 8; dir += 2)
				{
					auto flood{ sq_bit };
					auto stop{ ray[dir].boarder | blocker[slider[sl][sq].offset + p] };

					while (!(flood & stop))
					{
						bit::real_shift(flood, ray[dir].shift);
						new_attack |= flood;
					}
				}
				attack.push_back(new_attack);

				assert(attack.size() - 1 == slider[sl][sq].offset + p);
			}
		}
	}

	void init_magic(int sl, std::vector<uint64> &blocker, std::vector<uint64> &attack)
	{
		// finding fitting magic numbers
		// credits for the algorithm go to Tord Romstad:
		// https://www.chessprogramming.org/Looking_for_Magics

		assert(sl == ROOK || sl == BISHOP);
		rand_64xor rand_gen;
		bool fail{};

		for (int sq{ H1 }; sq <= A8; ++sq)
		{
			auto permutations{ 1 << (64 - slider[sl][sq].shift) };
			assert(permutations <= 4096);

			std::vector<uint64> save_attack;
			rand_gen.new_magic_seed(sq);
			do
			{
				do slider[sl][sq].magic = rand_gen.sparse64();
				while (bit::popcnt((slider[sl][sq].mask * slider[sl][sq].magic) & 0xff00000000000000) < 6);

				fail = false;
				save_attack.clear();
				save_attack.resize(permutations);

				for (int p{}; !fail && p < permutations; ++p)
				{
					auto index{ static_cast<int>(blocker[slider[sl][sq].offset + p] * slider[sl][sq].magic >> slider[sl][sq].shift) };
					assert(index <= permutations);

					if (!save_attack[index])
						 save_attack[index] = attack[slider[sl][sq].offset + p];

					else if (save_attack[index] != attack[slider[sl][sq].offset + p])
					{
						fail = true;
						break;
					}
				}
			} while (fail);
		}
	}

	void init_index(int sl, std::vector<uint64> &blocker, std::vector<uint64> &attack_temp)
	{
		// using the created magic numbers to index the attack table

		assert(sl == ROOK || sl == BISHOP);

		for (int sq{ H1 }; sq <= A8; ++sq)
		{
			auto permutations{ 1 << (64 - slider[sl][sq].shift) };
			for (int p{}; p < permutations; ++p)
			{
				attack_table[static_cast<uint32>(slider[sl][sq].offset
					+ (blocker[slider[sl][sq].offset + p] * slider[sl][sq].magic >> slider[sl][sq].shift))]
					= attack_temp[slider[sl][sq].offset + p];
			}
		}
	}
}

void magic::index_table()
{
	// indexing the attack table
	// starting by initializing temporary attack-tables & blocker-tables

	attack_table.clear();
	attack_table.resize(table_size);

	std::vector<uint64> attack_temp;
	attack_temp.reserve(table_size);

	std::vector<uint64> blocker;
	blocker.reserve(table_size);

	// generating magic numbers & indexing their final attack tables

	for (int sl{ ROOK }; sl <= BISHOP; ++sl)
	{
		init_mask(sl);
		init_blocker(sl, blocker);
		init_move( sl, blocker, attack_temp);
		init_magic(sl, blocker, attack_temp);
		init_index(sl, blocker, attack_temp);
	}
}
