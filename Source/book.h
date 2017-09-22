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

#include <fstream>

#include "position.h"
#include "main.h"

// reading from PolyGlot opening-books

class book
{
private:

	struct book_entry
	{
		uint64 key;
		uint16 move;
		uint16 count;
		uint16 n;
		uint16 sum;
	};

	static std::ifstream book_file;
	static int book_size;

	static int find_key(const uint64 &key);
	static void read_entry(book_entry &entry, int idx);

public:

	static std::string book_name;

	static bool open();

	static uint32 get_move(pos &board, bool best_line);
};
