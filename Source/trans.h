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

#include "position.h"
#include "main.h"

// managing the main transposition table

class trans
{
public:

	// a hash entry consists of 16 bytes

	struct hash
	{
		uint64 key;
		 int16 score;
		uint16 bounds;
		uint16 move;
		uint8 annex;
		uint8 depth;
	};

	static_assert(sizeof(hash) == 16, "tt entry != 16 bytes");

private:

	// actual hash table

	static hash* table;

	void erase();

public:

	// hash table properties

	static uint64 size;
	static uint64 mask;
	static uint64 hash_hits;

	trans(uint64 size)
	{
		erase();
		create(size);
	}

	~trans() { erase(); }

public:

	// manipulating the table

	int create(uint64 size);
	void clear();

	static uint64 to_key(const board &pos);

	static void store(const board &pos, uint32  move, int  score, int  bound, int depth, int curr_depth);
	static bool probe(const board &pos, uint32 &move, int &score, int &bound, int depth, int curr_depth);

	static int hashfull();
};