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

// attacking functions using bitboards

namespace attack
{
	// fixed rough value of pieces in centipawns

	constexpr int value[]{ 100, 325, 325, 500, 950, 10000, 0, 0 };

	// pre-calculated attack bitboards

	extern uint64 in_front[2][64];
	extern uint64 slide_map[2][64];
	extern uint64 knight_map[64];
	extern uint64 king_map[64];

	void fill_tables();

	// detecting check & finding pins and evasion squares

	uint64 check(const board &pos, int turn, uint64 all_sq);
	uint64 evasions(const board &pos);
	void pins(const board &pos, int side_king, int side_pin, uint64 pin_moves[]);

	// generating attacks

	uint64 by_piece(int piece, int sq, int side, const uint64 &occ);
	template<sliding_type sl> uint64 by_slider(int sq, uint64 occ);
	uint64 by_pawns(const board &pos, int turn);
	
	// doing a static exchange evaluation

	int see(const board &pos, uint32 move);
}