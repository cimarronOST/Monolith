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


#include "random.h"
#include "bit.h"
#include "magic.h"

magic::entry magic::slider[2][64];

std::vector<uint64> magic::attack_table;

const int magic::table_size{ 102400 + 5248 };

const magic::pattern magic::ray[]
{
	{  8, 0xff00000000000000 },{  7, 0xff01010101010101 },
	{ 63, 0x0101010101010101 },{ 55, 0x01010101010101ff },
	{ 56, 0x00000000000000ff },{ 57, 0x80808080808080ff },
	{  1, 0x8080808080808080 },{  9, 0xff80808080808080 }
};

void magic::index_table()
{
	// indexing the attack table

	attack_table.clear();
	attack_table.resize(table_size);

	std::vector<uint64> attack_temp;
	attack_temp.reserve(table_size);

	std::vector<uint64> blocker;
	blocker.reserve(table_size);

	// generating magic numbers & indexing their attack tables

	for (int sl{ ROOK }; sl <= BISHOP; ++sl)
	{
		init_mask(sl);
		init_blocker(sl, blocker);
		init_move(sl, blocker, attack_temp);
		init_magic(sl, blocker, attack_temp);
		init_index(sl, blocker, attack_temp);
	}
}

void magic::init_mask(int sl)
{
	// generating corresponding ray masks for each square

	assert(sl == ROOK || sl == BISHOP);

	for (int sq{ H1 }; sq <= A8; ++sq)
	{
		auto sq64{ 1ULL << sq };
		for (auto dir{ sl }; dir < 8; dir += 2)
		{
			auto flood{ sq64 };
			while (!(flood & ray[dir].boarder))
			{
				slider[sl][sq].mask |= flood;
				bit::real_shift(flood, ray[dir].shift);
			}
		}
		slider[sl][sq].mask ^= sq64;
	}
}

void magic::init_blocker(int sl, std::vector<uint64> &blocker)
{
	// generating all possible blocker boards for each square

	assert(sl == ROOK || sl == BISHOP);
	assert(blocker.size() == 0 || blocker.size() == 102400);

	bool bit[12]{ false };

	for (int sq{ H1 }; sq <= A8; ++sq)
	{
		slider[sl][sq].offset = blocker.size();
		auto mask{ slider[sl][sq].mask };

		int bits_set{ };
		uint64 split_mask[12]{ };
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
		for (auto p{ 0 }; p < permutations; ++p)
		{
			mask = 0ULL;
			for (auto b{ 0 }; b < bits_set; ++b)
			{
				if (!(p % (1 << b)))
					bit[b] = !bit[b];

				if (bit[b])
					mask |= split_mask[b];
			}
			blocker.push_back(mask);
		}
	}
}

void magic::init_move(int sl, std::vector<uint64> &blocker, std::vector<uint64> &attack)
{
	// generating all possible attacks from each square, considering the blocker boards

	assert(sl == ROOK || sl == BISHOP);
	assert(attack.size() == 0 || attack.size() == 102400);

	for (int sq{ H1 }; sq <= A8; ++sq)
	{
		auto sq64{ 1ULL << sq };

		auto permutations{ 1 << (64 - slider[sl][sq].shift) };
		for (auto p{ 0 }; p < permutations; ++p)
		{
			uint64 new_attack{ 0 };
			for (auto dir{ sl }; dir < 8; dir += 2)
			{
				auto flood{ sq64 };
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

void magic::init_magic(int sl, std::vector<uint64> &blocker, std::vector<uint64> &attack)
{
	// finding fitting magic numbers

	// credits go to Tord Romstad:
	// https://chessprogramming.wikispaces.com/Looking+for+Magics

	assert(sl == ROOK || sl == BISHOP);
	rand_64xor rand_gen;
	bool fail{ };

	for (int sq{ H1 }; sq <= A8; ++sq)
	{
		auto permutations{ 1 << (64 - slider[sl][sq].shift) };
		assert(permutations <= 4096);

		std::vector<uint64> att;
		rand_gen.new_magic_seed(sq >> 2);
		do
		{
			do
			{
				slider[sl][sq].magic = rand_gen.sparse64();
			} while (bit::popcnt((slider[sl][sq].mask * slider[sl][sq].magic) & 0xff00000000000000) < 6);

			fail = false;
			att.clear();
			att.resize(permutations);

			for (auto p{ 0 }; !fail && p < permutations; ++p)
			{
				auto idx{ static_cast<int>(blocker[slider[sl][sq].offset + p] * slider[sl][sq].magic >> slider[sl][sq].shift) };
				assert(idx <= permutations);

				if (!att[idx])
					att[idx] = attack[slider[sl][sq].offset + p];

				else if (att[idx] != attack[slider[sl][sq].offset + p])
				{
					fail = true;
					break;
				}
			}
		} while (fail);
	}
}

void magic::init_index(int sl, std::vector<uint64> &blocker, std::vector<uint64> &attack_temp)
{
	// using the created magic numbers to index the attack table

	assert(sl == ROOK || sl == BISHOP);

	for (int sq{ H1 }; sq <= A8; ++sq)
	{
		auto permutations{ 1 << (64 - slider[sl][sq].shift) };
		for (auto p{ 0 }; p < permutations; ++p)
		{
			attack_table[static_cast<uint32>(slider[sl][sq].offset + (blocker[slider[sl][sq].offset + p] * slider[sl][sq].magic >> slider[sl][sq].shift))]
			= attack_temp[slider[sl][sq].offset + p];
		}
	}
}