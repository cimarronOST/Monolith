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


#pragma once

#include "position.h"
#include "main.h"

// managing the main transposition hash table

class trans
{
private:

	struct hash
	{
		uint64 key;
		uint64 data;
	};

	static_assert(sizeof(hash) == 16, "tt entry != 16 bytes");

	// actual hash table & table properties

	static hash* table;

	static constexpr int slots{ 4 };
	static uint64 size;
	static uint64 mask;

	void erase();

public:

	static uint64 hash_hits;

	trans(uint64 size)
	{
		erase();
		create(size);
	}

	~trans() { erase(); }

	// type to store probing results

	struct entry
	{
		uint32 move;
		int score;
		int bound;
		int depth;
	};

	// manipulating the table

	int create(uint64 size);
	void clear();

	static uint64 to_key(const board &pos);

	static void store(uint64 &key, uint32 move, int score, int bound, int depth, int curr_depth);
	static bool probe(uint64 &key, entry &node, int depth, int curr_depth);

	static int hashfull();
};