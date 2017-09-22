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


#include <algorithm>

#include "attack.h"
#include "see.h"

uint64 see::lvp(const pos &board, const uint64 &set, int col, int &piece)
{
	// determining the least valuable piece

	for (piece = PAWNS; piece <= KINGS; ++piece)
	{
		uint64 subset{ set & board.pieces[piece] & board.side[col] };
		if (subset)
			return (subset & (subset - 1)) ^ subset;
	}
	return 0ULL;
}

int see::eval(const pos &board, uint32 move)
{
	// inspired by Michael Hoffmann

	int gain[32]{ 0 }, d{ 0 };
	uint64 all_xray{ board.pieces[PAWNS] | board.pieces[ROOKS] | board.pieces[BISHOPS] | board.pieces[QUEENS] };

	auto sq1{ to_sq1(move) };
	auto sq2{ to_sq2(move) };
	uint64 sq1_64{ 1ULL << sq1 };

	uint64 occ{ board.side[BOTH] };
	uint64 set{ attack::to_square(board, sq2) };
	uint64 gone{ 0ULL };
	int piece{ to_piece(move) };

	assert(to_victim(move) != NONE);
	gain[d] = exact_value[to_victim(move)];

	do
	{
		gain[++d] = exact_value[piece] - gain[d - 1];

		if (std::max(-gain[d - 1], gain[d]) < 0)
			break;

		set  ^= sq1_64;
		occ  ^= sq1_64;
		gone |= sq1_64;

		if (sq1_64 & all_xray)
			set |= attack::add_xray(board, occ, set, gone, sq2);

		sq1_64 = lvp(board, set, board.turn ^ (d & 1), piece);

	} while (sq1_64);

	while (--d) gain[d - 1] = -std::max(-gain[d - 1], gain[d]);

	return gain[0];
}
