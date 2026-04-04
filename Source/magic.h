/*
  Monolith Copyright (C) 2017-2026 Jonas Mayr

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

#include <array>
#include <vector>

#include "types.h"

// setting up the magic index function for fast move generation of sliding pieces
// the tables are indexed using the "fancy" approach:
// https://www.chessprogramming.org/Magic_Bitboards

namespace magic
{
	// to be calculated for each square:
	// attack mask, magic indexing number, offset of the array, shift of the magic key

	struct sq_entry
	{
		bit64 mask;
		bit64 magic;
		int offset;
		int shift;
	};

	enum piece { BISHOP, ROOK };

	// tables with all information for magic move generation after initialization

	inline std::array<std::array<sq_entry, 2>, 64> slider{};
	inline std::vector<bit64> attack_table{};

	// indexing the magic number attack tables

	void init_table();
}
