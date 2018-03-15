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

#include "engine.h"
#include "movesort.h"
#include "movegen.h"
#include "main.h"

// orchestrating the generation and sorting of the root node movelist

class movepick_root
{
public:

	gen list;
	sort_root sort;

	movepick_root(board &pos)
		: list(pos, LEGAL), sort(list)
	{
		if (engine::limit.moves.size())
			list.gen_searchmoves(engine::limit.moves);
		else
			list.gen_all();

		sort.statical();
	}

	// adjusting the root moves

	void rearrange_moves(uint32 pv_move, uint32 multipv_move);
	bool single_reply() const;
};

// orchestrating the generation and weighting of the movelist, and picking moves from it

class movepick
{
public:

	gen list;
	int attempts{ -1 };
	int move_idx{ -1 };

private:

	sort weight;
	gen_stage stage[5];

	struct count
	{
		int cycles{ -1 };
		int max_cycles{ };

		int attempts{ };
		int moves{ };
	} cnt;

public:

	// main search

	movepick(board &pos, uint32 tt_move, uint32 counter, sort::hist_list &history, sort::kill_list &killer, int depth)
		: list(pos, PSEUDO), weight(list, tt_move, counter, history, killer, depth)
	{
		cnt.max_cycles = 5;
		stage[0] = HASH;
		stage[1] = WINNING;
		stage[2] = KILLER;
		stage[3] = QUIET;
		stage[4] = LOOSING;
	}

	// quiescence search

	movepick(board &pos, bool in_check, int depth)
		: list(pos, LEGAL), weight(list)
	{
		cnt.max_cycles = 1 + (depth == 0);
		stage[0] = TACTICAL;
		stage[1] = CHECK;

		if (in_check)
		{
			cnt.max_cycles = 2;
			stage[1] = EVASION;
		}
	}

	// generating and weighting the moves

	void gen_weight();

public:

	// picking the highest weighted move
	// and if there are none left, initialising further generation and weighting

	uint32 next();
};