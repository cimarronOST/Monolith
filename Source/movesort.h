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

#include "movegen.h"
#include "position.h"
#include "main.h"

// handling the sorting of root node moves

class sort_root
{
private:

	board pos;
	gen &list;

	void sort_moves();

public:

	sort_root(gen &movelist) : pos(movelist.pos), list(movelist) {}

	// storing all relevant information of each move

	struct root_node
	{
		uint32 move;
		int64 nodes;
		int64 weight;
		bool check;
		bool skip;
	} root[lim::moves]{};

	// weighting the moves

	void statical();
	void statical_tb();
	void dynamical(uint32 pv_move);

	// handling multi-PV

	void exclude_move(uint32 multipv_move);
	void include_moves();
};

// assigning a weight to all moves in the alpha-beta search movelist for sorting purpose

class sort
{
public:

	// storing the assigned weights

	int64 score[lim::moves]{};

	// parameter only used in the main alpha-beta move weighting

	struct quiet_parameters
	{
		uint32 hash;
		uint32 counter;
		uint32 *killer;
		int (*history)[6][64];
	} quiets{};

	static constexpr int64 history_max{ 1ULL << 62 };

private:

	gen &list;

	// weighting captures & promotions

	int mvv_lva(uint32 move) const;
	int mvv_lva_promo(uint32 move) const;

	bool loosing(board &pos, uint32 move) const;
	bool previous(uint32 move) const;

public:

	// main search

	sort(gen &movelist, uint32 tt_move, uint32 counter, uint32 killer[], int history[][6][64]) : list(movelist)
	{
		quiets = { tt_move, counter, killer, history };
	}

	void hash();
	void tactical_see();
	void killer();
	void quiet();
	void loosing();
	void deferred();

	// quiescence search

	sort(gen &movelist) : list(movelist) {}

	void tactical();
	void evasion();
	void check();
};