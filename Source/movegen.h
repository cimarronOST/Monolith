/*
  Monolith 2 Copyright (C) 2017-2020 Jonas Mayr
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

#include "move.h"
#include "attack.h"
#include "bit.h"
#include "board.h"
#include "types.h"
#include "main.h"

// disable Intel compiler warnings
// explicitly instantiating template functions more than once

#if defined(__INTEL_COMPILER)
#pragma warning(disable : 803)
#endif

// generating moves
// generation is staged and can be legal or pseudo-legal

template<mode md> class gen
{
public:
	// fixed copy of the board position

	const board pos;

	// constructor decides whether generation will be legal or pseudo-legal

	gen(const board& fixed_pos) : pos{ fixed_pos }
	{
		if constexpr (md == mode::legal)
		{
			pin.find(pos, pos.cl, pos.cl);
			evasions = attack::evasions(pos);
		}
		if constexpr (md == mode::pseudolegal)
			verify(evasions == bit::max);
	}

	// list in which all generated moves are stored

	move_list mv{};
	struct count_moves
	{
		int mv;
		int capture;
		int promo;
		int loosing;
		int duplicate;
	} cnt{};
	bool empty() const { return !cnt.mv; }

private:
	// holding information for legal move-generation 

	bit64 evasions{ bit::max };
	attack::pin_mv pin{};

public:
	// generating staged moves as commanded by movepick

	int gen_all();
	int gen_searchmoves(const std::vector<move>& moves);
	int gen_hash(move& hash_mv);
	int gen_capture();
	int gen_promo(stage st);
	int gen_killer(const killer_list& killer, move counter);
	int gen_quiet();

	// restoring previously generated moves

	int restore_loosing();
	int restore_deferred(const move_list& deferred, int deferred_cnt);

private:
	// actual move generating functions

	template<stage st> void  pawns();
	template<stage st> void pieces(std::initializer_list<piece> pc);
	void castle();
};
