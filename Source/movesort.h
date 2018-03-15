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

#include "movegen.h"
#include "position.h"
#include "main.h"

// sorting root node moves

class sort_root
{
private:

	board pos;
	gen &list;

public:

	struct root_node
	{
		uint32 move;
		uint64 nodes;
		uint64 weight;
		bool check;
		bool skip;
	};

	sort_root(gen &movelist) : pos(movelist.pos), list(movelist){ }

	root_node root[lim::moves]{ };

	void statical();
	void dynamical(uint32 pv_move);
	void exclude_move(uint32 prev_move);
};

// assigning a weight to all moves in the alphabeta search movelist for sorting purpose

class sort
{
public:

	// history & killer heuristic types

	struct hist_list{ uint64 list[2][6][64]; };
	struct kill_list{ uint32 list[lim::depth][2]; };

	// storing the assigned weights

	uint64 score[lim::moves]{ };

	// parameter only used in the main alphabeta move weighting

	struct quiet_parameters
	{
		uint32 hash_move;
		uint32 counter;
		hist_list *history;
		kill_list *killer;
		int depth;
	} quiets;

private:

	gen &list;

private:

	// weighting captures & promotions

	int mvv_lva(uint32 move) const;
	int mvv_lva_promo(uint32 move) const;

	bool is_loosing(board &pos, uint32 move) const;
	bool is_double(uint32 move) const;

public:

	// main search

	sort(gen &movelist, uint32 tt_move, uint32 counter, hist_list &history, kill_list &killer, int depth) : list(movelist)
	{
		quiets = { tt_move, counter, &history, &killer, depth };
	}

	void hash();
	void tactical_see();
	void killer();
	void quiet();
	void loosing();

	// quiescence search

	sort(gen &movelist) : list(movelist) { }

	void tactical();
	void evasion();
	void check();
};