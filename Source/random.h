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


#pragma once

#include <random>

#include "types.h"
#include "main.h"

class rand_64xor
{
	// pseudo random number generation through xor-shift
	// used to generate "magic" index numbers

private:
	// using pre-calculated seeds to speed up magic number generation

	static constexpr std::array<bit64, 16> magic_seed
	{ {
		0x5dd4569, 0x33180c2, 0x1ab24ce, 0x4fc6fd8,
		0x559921d, 0x0db6850, 0x0c6e669, 0x4e47fcf,
		0x252b1fa, 0x4319b7f, 0x201818c, 0x3dd84f7,
		0x5ede0dc, 0x1321cc8, 0x2b9b062, 0x290b5b5
	} };

	bit64 seed{};
	bit64 rand64();

public:
	void new_seed(square sq);
	bit64 sparse64();
};

class rand_64
{
	// pseudo random number generation with standard library functions
	// used to generate Zobrist hash keys

private:
	std::mt19937_64 rand_gen;
	std::uniform_int_distribution<bit64> uniform{};

public:
	rand_64() : rand_gen(5489U) {}
	bit64 rand64();
};

class rand_32
{
	// pseudo random number generation with standard library functions
	// used to randomly choose an opening book move

private:
	std::random_device rd{};
	std::mt19937 rand_gen;

public:
	rand_32() : rand_gen(rd()) { }
	int rand32(int range);
};