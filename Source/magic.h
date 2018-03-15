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

#include <vector>

#include "main.h"

// setting up the magic index function for fast move generation of sliding pieces

class magic
{
private:

	struct entry
	{
		uint64 offset;
		uint64 mask;
		uint64 magic;
		int shift;
	};

	static const int table_size;

public:

	// confining the attacking rays depending on piece and direction

	struct pattern
	{
		int shift;
		uint64 boarder;
	};

	static const pattern ray[];

	// holding all magic information after the initialisation

	static entry slider[2][64];

	static std::vector<uint64> attack_table;

	// indexing magic number attack tables

	static void index_table();

private:

	static void init_mask(int sl);
	static void init_blocker(int sl, std::vector<uint64> &blocker);
	static void init_move(int sl, std::vector<uint64> &blocker, std::vector<uint64> &attack);

	static void init_magic(int sl, std::vector<uint64> &blocker, std::vector<uint64> &attack);
	static void init_index(int sl, std::vector<uint64> &blocker, std::vector<uint64> &attack);
};