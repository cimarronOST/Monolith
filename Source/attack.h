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

#include "position.h"
#include "main.h"

// bitboard attacking functions

namespace attack
{
	uint64 by_slider(const int sl, const int sq, uint64 occ);
	uint64 check(const pos &board, int turn, uint64 all_sq);

	uint64 by_pawns(const pos &board, int turn);

	// funtions for SEE

	uint64 to_square(const pos &board, uint16 sq);
	uint64 add_xray(const pos &board, uint64 &occ, uint64 &set, uint64 &gone, uint16 sq);
}
