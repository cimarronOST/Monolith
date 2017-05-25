/*
  Monolith 0.2  Copyright (C) 2017 Jonas Mayr

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

namespace book
{
	struct book_entry
	{
		uint64 key;
		uint16 move;
		uint16 count;
		uint16 n;
		uint16 sum;
	};

	bool open();
	uint16 get_move(pos &board);

	int find_key(const uint64 &key);
	void read_entry(book_entry &entry, int index);
};
