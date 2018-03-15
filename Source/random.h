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


#pragma once

#include <random>

#include "main.h"

// pseudo random number generation through xor-shift
// used to generate "magic" index numbers

class rand_64xor
{
private:

	uint64 seed;
	uint64 rand64();

public:

	void new_magic_seed(int idx);

	uint64 sparse64();
};

// pseudo random number generation through the standard library
// used to generate Zobrist hash keys

class rand_64
{
private:

	std::mt19937_64 rand_gen;
	std::uniform_int_distribution<uint64> uniform;

public:

	rand_64() : rand_gen(5489U) { }

	uint64 rand64();
};

// pseudo random number generation through the standard library
// used to randomly choose a opening book move

class rand_32
{
private:

	std::random_device rd;

	std::mt19937 rand_gen;
	std::uniform_int_distribution<int> uniform;

public:

	rand_32() : rand_gen(rd()) { }

	int rand32(int range);
};