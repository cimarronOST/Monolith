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

// managing the pawn hash table
// the pawn hash table speeds up the evaluation function

class pawn
{
private:

	// size of { 1ULL << 11 } correlates to a fixed pawn-hash table of ~65 KB per thread

	constexpr static uint64 size{ 1ULL << 11 };
	void clear();

public:

	// pawn hash entry is 32 bytes

	struct hash
	{
		uint64 key;
		uint64 passed[2];
		int16 score[2][2];
	};

	static_assert(sizeof(hash) == 32, "pawnhash entry != 32 bytes");

	// actual table & properties

	hash *table{};
	constexpr static uint64 mask{ size - 1 };

	static uint64 to_key(const board &pos);

	// creating & destroying the table

	pawn(bool allocate)
	{
		if (allocate)
		{
			table = new hash[static_cast<uint32>(size)];
			clear();
		}
	}

	~pawn()
	{
		if (table != nullptr)
		{
			delete[] table;
			table = nullptr;
		}
	}
};