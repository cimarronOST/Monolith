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


#pragma once

#include "sort.h"
#include "movegen.h"
#include "main.h"

// generating, weighting and picking moves from the movelist

class movepick
{
public:

	pos reverse;
	movegen list;

private:

	sort weight;

	GEN_STAGE stage[4]{};

	struct count
	{
		int cycles{ -1 };
		int max_cycles{ 0 };

		int attempts{ 0 };
		int moves{ 0 };
	} cnt;

public:

	// root search

	movepick(pos &board, uint64 nodes[])
		: reverse(board), list(board, LEGAL), weight(list, nodes)
	{
		list.gen_tactical();
		list.gen_quiet();

		cnt.cycles = 0;
		cnt.max_cycles = 0;
		cnt.attempts = cnt.moves = list.cnt.moves;

		weight.root_static(nodes);
	}

	// main search

	movepick(pos &board, uint32 tt_move, sort::history_list &history, sort::killer_list &killer, int depth)
		: reverse(board), list(board, PSEUDO, tt_move), weight(list, history, killer, depth)
	{
		cnt.max_cycles = 4;
		stage[0] = HASH;
		stage[1] = WINNING;
		stage[2] = QUIET;
		stage[3] = LOOSING;
	}

	// quiescence search

	movepick(pos &board)
		: reverse(board), list(board, LEGAL), weight(list)
	{
		cnt.max_cycles = 1;
		stage[0] = TACTICAL;
	}

	// generating and weighting the moves

	void gen_weight();

public:

	void rearrange_root(uint64 nodes[], uint32 *pv_move);

	// picking the highest weighted move
	// and if there are none left, initialising further generation and sorting

	uint32 *next();
};
