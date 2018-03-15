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


#include "bit.h"
#include "zobrist.h"
#include "pawn.h"

pawn::hash *pawn::table;

const uint64 pawn::size{ 1ULL << 11 };
const uint64 pawn::mask{ size - 1 };

void pawn::hash::clear()
{
	*this = { 0ULL, {0ULL,0ULL}, {{0,0},{0,0}} };
}

void pawn::clear()
{
	// clearing the table

	for (auto i{ 0U }; i < size; ++i)
		table[i].clear();
}

uint64 pawn::to_key(const board &pos)
{
	// converting the position into a pawn hash key
	// the key considers only squares occupied by pawns

	uint64 key{ 0ULL };

	for (int col{ WHITE }; col <= BLACK; ++col)
	{
		auto pawns{ pos.pieces[PAWNS] & pos.side[col] };
		while (pawns)
		{
			auto sq{ bit::scan(pawns) };
			assert(pos.piece_sq[sq] == PAWNS);

			key ^= zobrist::rand_key[col * 64 + sq];
			pawns &= pawns - 1;
		}
	}
	return key;
}