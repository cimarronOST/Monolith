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

#include "movegen.h"
#include "position.h"
#include "main.h"

// assigning a weight to all moves in the movelist for sorting purpose

class sort
{
public:

	// history & killer heuristic types

	struct history_list
	{
		uint64 list[2][6][64];
	};

	struct killer_list
	{
		uint32 list[lim::depth][2];
	};

	// storing the assigned weights

	uint64 score[lim::movegen]{ };

private:

	movegen &list;

	// for objects of the main alphabeta routine

	struct main_parameters
	{
		int depth;
		killer_list *killer;
		history_list *history;
	} main;

private:

	// weighting captures & promotions

	int mvv_lva(uint32 move) const;
	int mvv_lva_promo(uint32 move) const;

	bool loosing(pos &board, uint32 move) const;

public:

	// root search objects

	sort(movegen &movelist, uint64 nodes[]) : list(movelist) { }

	// main search objects

	sort(movegen &movelist, history_list &history, killer_list &killer, int depth) : list(movelist)
	{
		main.depth = depth;
		main.killer = &killer;
		main.history = &history;
	}

	// qsearch objects

	sort(movegen &movelist) : list(movelist) { }

	// weighting qsearch & main search

	void hash(GEN_STAGE stage);
	void tactical_see(GEN_STAGE stage);
	void tactical();
	void quiet();
	void loosing();

	// weighting root search

	void root_static(uint64 nodes[]);
	void root_dynamic(uint64 nodes[], uint32 *pv_move);
};
