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

// Zobrist hashing used for transposition table & pawn table & endgame tablebases & repetition detection

namespace zobrist
{
	extern uint64 rand_key[781];

	constexpr struct offset
	{
		int castling;
		int ep;
		int turn;
	} off{ 768, 772, 780 };
	
	void init_keys();
	uint64 material_key(board& pos, int mirror);
	uint64 material_key(int pieces[], int mirror);
}