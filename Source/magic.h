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


#pragma once

#include <vector>

#include "main.h"

// setting up the magic index function for fast move-generation of sliding pieces

class magic
{
private:

	struct entry
	{
		size_t offset;
		uint64 mask;
		uint64 magic;
		int shift;
	};

public:

	struct pattern
	{
		int shift;
		uint64 boarder;
	};

	// variables to fill

	static entry slider[2][64];

	static std::vector<uint64> attack_table;

	static const int table_size;
	static const pattern ray[];

	// generating & indexing magic numbers

	static void init();

private:

	static void init_mask(int sl);
	static void init_blocker(int sl, std::vector<uint64> &blocker);
	static void init_move(int sl, std::vector<uint64> &blocker, std::vector<uint64> &attack_temp);

	static void init_magic(int sl, std::vector<uint64> &blocker, std::vector<uint64> &attack_temp);
	static void connect(int sl, std::vector<uint64> &blocker, std::vector<uint64> &attack_temp);
};
