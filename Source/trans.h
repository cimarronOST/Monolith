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

#include "move.h"
#include "types.h"
#include "main.h"

// managing the main transposition hash table

class trans
{
private:
	struct hash
	{
		key64 key;
		uint64 data;
	};

	static_assert(sizeof(hash) == 16);

	// hash table & table properties

	struct std_free { void operator()(hash* t) { std::free(t); } };
	static std::unique_ptr<hash[], std_free> table;

	static constexpr int slots{ 4 };
	static uint64 size;
	static uint64 mask;

public:
	// storing and probing

	struct entry
	{
		move  mv{};
		score sc{ score::none };
		bound bd{ bound::none };
		depth dt{};

		bool probe(const key64& key, depth curr_dt);
	};

	static hash* get_entry(const key64& key);
	static void store(const key64& key, move mv, score sc, bound bd, depth remaining_dt, depth curr_dt);

	// manipulating the table

	static std::size_t create(std::size_t megabytes);
	void clear();
	
	static int hashfull();
};