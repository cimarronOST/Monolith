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

#include "main.h"

// concerning move encoding

namespace move
{
	// encoding a move into 32 bits

	struct elements
	{
		int sq1;
		int sq2;
		int flag;
		int piece;
		int victim;
		int turn;
	};

	int sq1(uint32 move);
	int sq2(uint32 move);
	int flag(uint32 move);
	int piece(uint32 move);
	int victim(uint32 move);
	int turn(uint32 move);

	elements decode(uint32 move);
	uint32 encode(int sq1, int sq2, int flag, int piece, int victim, int turn);

	// determining some move properties

	bool is_castling(int flag);
	bool is_castling(uint32 move);
	bool is_promo(uint32 move);
	bool is_quiet(uint32 move);

	bool is_push_to_7th(uint32 move);
	bool is_pawn_advance(uint32 move);
}