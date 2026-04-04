/*
  Monolith Copyright (C) 2017-2026 Jonas Mayr

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

#include <array>

#include "history.h"
#include "move.h"
#include "movegen.h"
#include "board.h"
#include "types.h"

// handling the sorting of root node moves

class rootsort
{
private:
	board pos;
	const gen<mode::LEGAL>& list;

public:
	rootsort(const gen<mode::LEGAL>& mv_list) : pos{ mv_list.pos }, list{ mv_list } {}

	// storing all relevant information of each move

	struct root_node
	{
		int64 nodes{};
		int64 weight{};
		move mv{};
		bool check{};
		bool skip{};
	};
	std::array<root_node, lim::moves> root{};

	// weighting the moves

	void sort_moves();
	void sort_static();
	void sort_dynamic(move pv_mv);

	// handling multi-PV

	void exclude_move(move multipv_mv);
	void include_moves();
};

// assigning a weight to all moves for sorting purpose in the main alpha-beta search

template<mode md> class sort
{
private:
	gen<md>& list;

public:
	// storing the assigned weights

	std::array<uint32, lim::moves> sc{};

	// parameters only used in the main alpha-beta move weighting

	struct node_parameter
	{
		move hash;
		move counter;
		const sstack* stack;
		const killer_list* killer;
		const history* hist;
	} node{};

	score see_margin{ score::NONE };

private:
	// weighting captures & promotions

	uint32 mvv_lva(move mv) const;
	uint32 mvv_lva_promo(move mv) const;

	bool previous(move mv) const;

public:
	// main search

	sort(gen<md>& mvlist, move mv_tt, const sstack* stack, move counter, const history& hist_tables)
		: list{ mvlist }, node{ mv_tt, counter, stack, &stack->killer, &hist_tables } {}

	void hash();
	void tactical_see();
	void killer();
	void quiet();
	void loosing();

	// quiescence search

	sort(gen<md> &mvlist) : list{ mvlist } {}

	void tactical();
	void evasion();
};