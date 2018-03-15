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

// managing the pawn hash table

class pawn
{
public:

	// pawn hash entry is 32 bytes

	struct hash
	{
		uint64 key;
		uint64 passed[2];
		int16 score[2][2];
		void clear();
	};

	static_assert(sizeof(hash) == 32, "pawnhash entry > 32 bytes");

	// actual table & properties

	static hash *table;

	const static uint64 size;
	const static uint64 mask;

	static uint64 to_key(const board &pos);

	// creating & destroying the table

	pawn()
	{
		table = new hash[size];
		clear();
	}

	~pawn()
	{
		if (table != nullptr)
		{
			delete[] table;
			table = nullptr;
		}
	}

	void clear();
};