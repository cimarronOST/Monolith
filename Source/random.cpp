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

void rand_64xor::new_seed(square sq)
{
	// getting a new seed for the PRNG depending on the square
	// this speeds up magic number generation significantly; idea from Marco Costalba:
	// http://www.talkchess.com/forum3/viewtopic.php?t=39298

	verify(type::sq(sq));
	seed = magic_seed[sq >> 2];
}

bit64 rand_64xor::rand64()
{
	// xor-shift pseudo random number generation
	// idea from George Marsaglia:
	// https://www.jstatsoft.org/article/view/v008i14

	seed ^= seed >> 12;
	seed ^= seed << 25;
	seed ^= seed >> 27;
	return  seed * 0x2545f4914f6cdd1dULL;
}

uint64 rand_64xor::sparse64()
{
	// creating a sparse magic number

	return rand64() & rand64() & rand64();
}

bit64 rand_64::rand64()
{
	// creating a Zobrist hash key

	return uniform(rand_gen);
}

int rand_32::rand32(int range)
{
	// creating a pseudo-random number within the range

	return rand_gen() % range;
}