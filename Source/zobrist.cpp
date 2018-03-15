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
#include "zobrist.h"

uint64 zobrist::rand_key[781];

const struct zobrist::offset zobrist::off{ 768, 772, 780 };

void zobrist::init_keys()
{
	// generating Zobrist hash keys

	rand_64 rand_gen;

	for (auto &key : rand_key)
		key = rand_gen.rand64();
}