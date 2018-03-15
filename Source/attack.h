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

// bitboard attacking functions

class attack
{
public:

	// attacking bitboards

	static uint64 in_front[2][64];
	static uint64 slide_map[2][64];
	static uint64 knight_map[64];
	static uint64 king_map[64];

	static void fill_tables();

	// detecting check & generating attacks

	static uint64 check(const board &pos, int turn, uint64 all_sq);

	template<sliding_type sl> static uint64 by_slider(int sq, uint64 occ);
	static uint64 by_pawns(const board &pos, int turn);

	// assisting SEE

	static uint64 to_square(const board &pos, int sq);
	static uint64 add_xray(const board &pos, int sq, uint64 &occ);
};