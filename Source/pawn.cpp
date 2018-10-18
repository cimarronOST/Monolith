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


#include "bit.h"
#include "zobrist.h"
#include "pawn.h"

void pawn::clear()
{
	// clearing the table

	for (uint32 i{}; i < size; ++i)
		table[i] = hash{};
}

uint64 pawn::to_key(const board &pos)
{
	// converting the position into a pawn hash key
	// the key considers only squares occupied by pawns

	uint64 key{};
	for (int color{ WHITE }; color <= BLACK; ++color)
	{
		auto pawns{ pos.pieces[PAWNS] & pos.side[color] };
		while (pawns)
		{
			auto sq{ bit::scan(pawns) };
			assert(pos.piece[sq] == PAWNS);

			key ^= zobrist::rand_key[(PAWNS * 2 + color) * 64 + sq];
			pawns &= pawns - 1;
		}
	}
	return key;
}