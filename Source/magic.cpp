/*
  Monolith 0.3  Copyright (C) 2017 Jonas Mayr

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
#include "bitboard.h"
#include "magic.h"

magic::entry magic::slider[2][64];

std::vector<uint64> magic::attack_table;
const int magic::table_size{ 107648 };

const magic::pattern magic::ray[]
{
	{ 8,  0xff00000000000000 },{ 7,  0xff01010101010101 },
	{ 63, 0x0101010101010101 },{ 55, 0x01010101010101ff },
	{ 56, 0x00000000000000ff },{ 57, 0x80808080808080ff },
	{ 1,  0x8080808080808080 },{ 9,  0xff80808080808080 }
};

namespace
{
	inline void real_shift(uint64 &bb, int shift)
	{
		bb = (bb << shift) | (bb >> (64 - shift));
	}
}

void magic::init()
{
	// generating & indexing magic numbers

	attack_table.clear();
	attack_table.resize(table_size);

	std::vector<uint64> attack_temp;
	attack_temp.reserve(table_size);

	std::vector<uint64> blocker;
	blocker.reserve(table_size);

	for (int sl{ ROOK }; sl <= BISHOP; ++sl)
	{
		init_mask(sl);
		init_blocker(sl, blocker);
		init_move(sl, blocker, attack_temp);
		init_magic(sl, blocker, attack_temp);
		connect(sl, blocker, attack_temp);
	}
}

void magic::init_mask(int sl)
{
	assert(sl == ROOK || sl == BISHOP);

	for (int sq{ 0 }; sq < 64; ++sq)
	{
		uint64 sq64{ 1ULL << sq };

		for (int dir{ sl }; dir < 8; dir += 2)
		{
			uint64 flood{ sq64 };
			while (!(flood & ray[dir].boarder))
			{
				slider[sl][sq].mask |= flood;
				real_shift(flood, ray[dir].shift);
			}
		}
		slider[sl][sq].mask ^= sq64;
	}
}

void magic::init_blocker(int sl, std::vector<uint64> &blocker)
{
	assert(sl == ROOK || sl == BISHOP);
	assert(blocker.size() == 0 || blocker.size() == 102400);

	bool bit[12]{ false };

	for (int sq{ 0 }; sq < 64; ++sq)
	{
		slider[sl][sq].offset = blocker.size();

		uint64 mask_split[12]{ 0 };
		int bits_in{ 0 };

		uint64 mask_bit{ slider[sl][sq].mask };
		while (mask_bit)
		{
			mask_split[bits_in++] = 1ULL << bb::bitscan(mask_bit);

			mask_bit &= mask_bit - 1;
		}
		assert(bits_in <= 12);
		assert(bits_in >= 5);
		assert(bb::popcnt(slider[sl][sq].mask) == bits_in);

		slider[sl][sq].shift = 64 - bits_in;

		int max{ 1 << bits_in };
		for (int a{ 0 }; a < max; ++a)
		{
			uint64 board{ 0 };
			for (int b{ 0 }; b < bits_in; ++b)
			{
				if (!(a % (1 << b)))
					bit[b] = !bit[b];
				if (bit[b])
					board |= mask_split[b];
			}
			blocker.push_back(board);
		}
	}
}

void magic::init_move(int sl, std::vector<uint64> &blocker, std::vector<uint64> &attack_temp)
{
	assert(sl == ROOK || sl == BISHOP);
	assert(attack_temp.size() == 0 || attack_temp.size() == 102400);

	for (int sq{ 0 }; sq < 64; ++sq)
	{
		uint64 sq64{ 1ULL << sq };

		int max{ 1 << (64 - slider[sl][sq].shift) };
		for (int cnt{ 0 }; cnt < max; ++cnt)
		{
			uint64 board{ 0 };

			for (int dir{ sl }; dir < 8; dir += 2)
			{
				uint64 flood{ sq64 };
				while (!(flood & ray[dir].boarder) && !(flood & blocker[slider[sl][sq].offset + cnt]))
				{
					real_shift(flood, ray[dir].shift);
					board |= flood;
				}
			}
			attack_temp.push_back(board);

			assert(attack_temp.size() - 1 == slider[sl][sq].offset + cnt);
		}
	}
}

void magic::init_magic(int sl, std::vector<uint64> &blocker, std::vector<uint64> &attack_temp)
{
	// pre-calculated seeds to speed up magic number generation

	const uint64 seeds[]
	{
		908859, 953436, 912753, 482262, 322368, 711868, 839234, 305746,
		711822, 703023, 270076, 964393, 704635, 626514, 970187, 398854
	};

	// generating magic numbers, inspired by Tord Romstad

	bool fail;
	for (int sq{ 0 }; sq < 64; ++sq)
	{
		int occ_size{ 1 << (64 - slider[sl][sq].shift) };
		assert(occ_size <= 4096);

		std::vector<uint64> occ;
		occ.resize(occ_size);

		rand_xor rand_gen{ seeds[sq >> 2] };

		do
		{
			do slider[sl][sq].magic = rand_gen.sparse64();
			while (bb::popcnt((slider[sl][sq].mask * slider[sl][sq].magic) & 0xff00000000000000) < 6);

			fail = false;
			occ.clear();
			occ.resize(occ_size);

			for (int i{ 0 }; !fail && i < occ_size; ++i)
			{
				int idx{ static_cast<int>(blocker[slider[sl][sq].offset + i] * slider[sl][sq].magic >> slider[sl][sq].shift) };
				assert(idx <= occ_size);

				if (!occ[idx])
					occ[idx] = attack_temp[slider[sl][sq].offset + i];

				else if (occ[idx] != attack_temp[slider[sl][sq].offset + i])
				{
					fail = true;
					break;
				}
			}
		} while (fail);
	}
}

void magic::connect(int sl, std::vector<uint64> &blocker, std::vector<uint64> &attack_temp)
{
	assert(sl == ROOK || sl == BISHOP);

	for (int sq{ 0 }; sq < 64; ++sq)
	{
		int max{ 1 << (64 - slider[sl][sq].shift) };

		for (int cnt{ 0 }; cnt < max; ++cnt)
		{
			attack_table
			[
				static_cast<uint32>(slider[sl][sq].offset
					+ (blocker[slider[sl][sq].offset + cnt] * slider[sl][sq].magic >> slider[sl][sq].shift))
			]
			= attack_temp[slider[sl][sq].offset + cnt];
		}
	}
}
