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

#include "uci.h"
#include "movesort.h"
#include "movegen.h"
#include "main.h"

// orchestrating the generation and sorting of the root node movelist

class rootpick
{
public:

	gen list;
	sort_root sort;
	bool tb_pos{};

	rootpick(board &pos) : list(pos, LEGAL), sort(list)
	{
		if (uci::limit.moves.size())
			list.gen_searchmoves(uci::limit.moves);
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
	int hits{};

private:

	sort weight;
	gen_stage stage[6]{};
	uint32 *deferred_moves;

	struct move_count
	{
		int cycles{ -1 };
		int max_cycles{};

		int attempts{};
		int moves{};
	} count;
	
	// generating and weighting the moves

	void gen_weight();

public:

	// alpha-beta search

	movepick(board &pos, uint32 tt_move, uint32 counter, uint32 killer[], int history[][6][64], uint32 deferred[])
		: list(pos, PSEUDO), weight(list, tt_move, counter, killer, history), deferred_moves(deferred)
	{
		count.max_cycles = 5 + (uci::thread_count > 1);
		stage[0] = HASH;
		stage[1] = WINNING;
		stage[2] = KILLER;
		stage[3] = QUIET;
		stage[4] = LOOSING;
		stage[5] = DEFERRED;
	}

	// quiescence search

	movepick(board &pos, bool in_check, int depth)
		: list(pos, LEGAL), weight(list), deferred_moves(nullptr)
	{
		count.max_cycles = 1 + (depth == 0);
		stage[0] = TACTICAL;
		stage[1] = CHECK;

		if (in_check)
		{
			count.max_cycles = 2;
			stage[1] = EVASION;
		}
	}

public:

	void revert(board &pos);
	bool can_defer() const;

	// picking the highest weighted move
	// and if there is none left, initializing further generation and weighting

	uint32 next();
};