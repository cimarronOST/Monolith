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
#include "utilities.h"
#include "zobrist.h"

uint64 zobrist::rand_key[781]{};

void zobrist::init_keys()
{
	// generating Zobrist hash keys

	rand_64 rand_gen;
	for (auto &key : rand_key)
		key = rand_gen.rand64();
}

uint64 zobrist::material_key(board& pos, int mirror)
{
	// producing a 64-bit material signature key from the position, used to probe tablebases
	
	uint64 key{};
	int color{ !mirror ? WHITE : BLACK };
	for (int piece{ PAWNS }; piece <= KINGS; ++piece)
	{
		for (auto i{ bit::popcnt(pos.pieces[piece] & pos.side[color]) }; i > 0; --i)
			key ^= zobrist::rand_key[(piece * 2 + WHITE) * 64 + i - 1];
	}
	color ^= 1;
	for (int piece{ PAWNS }; piece <= KINGS; ++piece)
	{
		for (auto i{ bit::popcnt(pos.pieces[piece] & pos.side[color]) }; i > 0; --i)
			key ^= zobrist::rand_key[(piece * 2 + BLACK) * 64 + i - 1];
	}
	return key;
}

uint64 zobrist::material_key(int pieces[], int mirror)
{
	// producing a 64-bit material key corresponding to the material combination defined by pieces[16],
	// where indices 1 -  6 correspond to the number of white pawns - kings
	// and   indices 9 - 14 to the number of black pawns - kings
	// used to probe tablebases

	uint64 key{};
	auto color{ !mirror ? 0 : 8 };
	for (int piece{ PAWNS }; piece <= KINGS; ++piece)
	{
		for (int i{}; i < pieces[color + 1 + piece]; ++i)
			key ^= zobrist::rand_key[(piece * 2 + WHITE) * 64 + i];
	}
	color ^= 8;
	for (int piece{ PAWNS }; piece <= KINGS; ++piece)
	{
		for (int i{}; i < pieces[color + 1 + piece]; ++i)
			key ^= zobrist::rand_key[(piece * 2 + BLACK) * 64 + i];
	}
	return key;
}