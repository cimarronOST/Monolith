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

#include <vector>

#include "position.h"
#include "main.h"

// staged move generation, can be legal or pseudolegal

class gen
{
public:

	board pos;

	gen(board &position, gen_mode mode) : pos(position)
	{
		if (mode == LEGAL)
		{
			find_pins();
			find_evasions();
		}
		else
		{
			assert(mode == PSEUDO);
			evasions = ~0ULL;
		}
	}

private:

	// speeding up minor piece check generation

	uint64 king_color{ ~0ULL };

	// preparing legal moves

	uint64 pin[64]{ };
	uint64 evasions;

	void find_pins();
	void find_evasions();

public:

	// movelist

	uint32 move[lim::moves];

	struct count
	{
		int moves{ };
		int capture{ };
		int promo{ };
		int loosing{ };
	} cnt;

	bool in_list(uint32 move);
	uint32 *find(uint32 move);

	// generating staged moves as commanded by movepick

	void gen_all();
	void gen_searchmoves(std::vector<uint32> &searchmoves);

	int gen_hash(uint32 &hash_move);
	template<promo_mode promo> int gen_tactical();
	int gen_killer(uint32 killer[][2], int depth, uint32 counter);
	int gen_quiet();
	int gen_loosing();
	int gen_check();

private:

	// actual move generation functions

	template<promo_mode promo> void pawn_promo();
	void pawn_capt();
	void pawn_quiet(uint64 mask);

	void knight(uint64 mask);
	void bishop(uint64 mask);
	void rook(uint64 mask);
	void queen(uint64 mask);
	void king(uint64 mask);
};