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

#include "main.h"

// setting up the magic index function for fast move generation of sliding pieces
// the tables are indexed using the "fancy" approach

namespace magic
{
	// each square gets an entry

	struct square_entry
	{
		uint64 mask;
		uint64 magic;
		int offset;
		int shift;
	};

	// confining the attacking rays depending on piece and direction

	struct pattern
	{
		int shift;
		uint64 boarder;
	};

	constexpr pattern ray[]
	{
		{  8, 0xff00000000000000 }, {  7, 0xff01010101010101 },
		{ 63, 0x0101010101010101 }, { 55, 0x01010101010101ff },
		{ 56, 0x00000000000000ff }, { 57, 0x80808080808080ff },
		{  1, 0x8080808080808080 }, {  9, 0xff80808080808080 }
	};

	// tables that are holding all magic information after the initialization

	extern square_entry slider[2][64];
	extern std::vector<uint64> attack_table;

	// indexing magic number attack tables

	void index_table();
}