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

// using pre-calculated seeds to speed up magic number generation

const uint64 rand_64xor::magic_seed[16]
{
	0x5dd4569, 0x33180c2, 0x1ab24ce, 0x4fc6fd8, 0x559921d, 0x0db6850, 0x0c6e669, 0x4e47fcf,
	0x252b1fa, 0x4319b7f, 0x201818c, 0x3dd84f7, 0x5ede0dc, 0x1321cc8, 0x2b9b062, 0x290b5b5
};

void rand_64xor::new_magic_seed(int sq)
{
	// getting a new seed for the PRNG depending on the square

	assert(H1 <= sq && sq <= A8);
	seed = magic_seed[sq >> 2];
}

uint64 rand_64xor::rand64()
{
	// xor-shift pseudo random number generation
	// credit for the generator goes to George Marsaglia:
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

uint64 rand_64::rand64()
{
	// creating a Zobrist hash key

	return uniform(rand_gen);
}

int rand_32::rand32(int range)
{
	// creating a pseudo-random number within <range>

	return uniform(rand_gen) % range;
}

int relative::rank(int rank, int side)
{
	// relativizing the rank index perspective

	assert(rank >= R1 && rank <= R8);
	assert(side == WHITE || side == BLACK);
	return side == WHITE ? rank : 7 - rank;
}

int value::minmax(int value, int min, int max)
{
	// confining <value> between <min> and <max>

	assert(min < max);
	return std::min(max, std::max(min, value));
}

int index::file(int sq)
{
	// determining the square's file

	assert(sq >= H1 && sq <= A8);
	return sq & 7;
}

int index::rank(int sq)
{
	// determining the square's rank

	assert(sq >= H1 && sq <= A8);
	return sq >> 3;
}

int square::flip(int sq)
{
	// adjusting the square to a flipped board

	assert(sq >= H1 && sq <= A8);
	return sq + 7 - 2 * index::file(sq);
}

int square::distance(int sq1, int sq2)
{
	// determining the shortest distance between two squares

	assert(sq1 >= H1 && sq1 <= A8);
	assert(sq2 >= H1 && sq2 <= A8);
	return std::max(std::abs(index::file(sq1) - index::file(sq2)), std::abs(index::rank(sq1) - index::rank(sq2)));
}

int square::index(std::string &sq)
{
	// estracting the square-index of a move-string, e.g. 'h1' = 0, 'a8' = 63

	assert(sq.size() == 2);
	return 'h' - sq.front() + ((sq.back() - '1') << 3);
}