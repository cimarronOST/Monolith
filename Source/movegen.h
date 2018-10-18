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

#include "attack.h"
#include "position.h"
#include "main.h"

// staged move generation, can generate both legal and pseudolegal moves

class gen
{
public:

	board pos;

	gen(board &position, gen_mode mode) : pos(position)
	{
		if (mode == LEGAL)
		{
			attack::pins(pos, pos.turn, pos.turn, pin);
			evasions = attack::evasions(pos);
		}
		else
		{
			assert(mode == PSEUDO);
			assert(evasions == std::numeric_limits<uint64>::max());
		}
	}

private:

	// speeding up minor piece check generation & holding information for legal move-generation

	uint64 king_color{ std::numeric_limits<uint64>::max() };
	uint64 evasions{   std::numeric_limits<uint64>::max() };
	uint64 pin[64]{};

public:

	// movelist

	uint32 move[lim::moves]{};
	int moves{};
	struct move_count
	{
		int capture;
		int promo;
		int loosing;
	} count{};

	bool find(uint32 move);
	bool empty() const;

	// generating staged moves as commanded by movepick

	void gen_all();
	void gen_searchmoves(std::vector<uint32> &searchmoves);

	int gen_hash(uint32 &hash_move);
	template<promo_mode promo> int gen_tactical();
	int gen_killer(uint32 killer[], uint32 counter);
	int gen_quiet();
	int gen_check();

	int restore_loosing();
	int restore_deferred(uint32 deferred[]);

private:

	// actual move generating functions

	template<promo_mode promo> void pawn_promo();
	void pawn_capture();
	void pawn_quiet(uint64 mask);

	void knight(uint64 mask);
	void bishop(uint64 mask);
	void rook(uint64 mask);
	void queen(uint64 mask);
	void king(uint64 mask);
};