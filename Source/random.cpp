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

namespace
{
	// using precalculated seeds to speed up magic number generation

	const uint64 magic_seed[]
	{
		0x5dd4569, 0x33180c2, 0x1ab24ce, 0x4fc6fd8, 0x559921d, 0x0db6850, 0x0c6e669, 0x4e47fcf,
		0x252b1fa, 0x4319b7f, 0x201818c, 0x3dd84f7, 0x5ede0dc, 0x1321cc8, 0x2b9b062, 0x290b5b5
	};
}

void rand_64xor::new_magic_seed(int idx)
{
	assert(idx >= 0 && idx < 16);
	seed = magic_seed[idx];
}

uint64 rand_64xor::rand64()
{
	// xor-shift pseudo random number generation

	// credit goes to George Marsaglia:
	// https://www.jstatsoft.org/article/view/v008i14

	seed ^= seed >> 12;
	seed ^= seed << 25;
	seed ^= seed >> 27;
	return seed * 0x2545f4914f6cdd1dULL;
}

uint64 rand_64xor::sparse64()
{
	// creating a sparse magic number

	return rand64() & rand64() & rand64();
}

uint64 rand_64::rand64()
{
	// creating a Zobrist hash key

	return uniform(rand_gen);
}

int rand_32::rand32(int range)
{
	// creating a pseudorandom number within <range>

	return uniform(rand_gen) % range;
}