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
#include "movegen.h"
#include "board.h"
#include "types.h"
#include "main.h"

// handling the history tables

class history
{
private:
	// updating the history tables

	void update_entry(int& entry, int weight);
	void set_main(move mv, int weight);
	void set_counter(move mv, move mv_prev, int weight);
	void set_continuation(move mv, move mv_prev, int weight);

public:
	static constexpr int max{ 0x2aaaaaaa };

	std::array<std::array<std::array<int, 64>, 6>, 2> main{};
	std::array<std::array<std::array<std::array<int, 64>, 6>, 64>, 6> counter{};
	std::array<std::array<std::array<std::array<int, 64>, 6>, 64>, 6> continuation{};

	void update(move mv, move mv_prev1, move mv_prev2, const move_list& quiet_mv, int quiet_cnt, depth dt);
	void clear();

	// probing the history tables

	struct sc
	{
		int main;
		int counter;
		int continuation;
		void get(const history& hist, move mv, move mv_prev1, move mv_prev2);
	};
};

// handling the sorting of root node moves

class rootsort
{
private:
	board pos;
	const gen<mode::legal>& list;

public:
	rootsort(const gen<mode::legal>& mvlist) : pos{ mvlist.pos }, list{ mvlist } {}

	// storing all relevant information of each move

	struct root_node
	{
		move mv{};
		int64 nodes{};
		int64 weight{};
		bool check{};
		bool skip{};
	};
	std::array<root_node, lim::moves> root{};

	// weighting the moves

	void sort_moves();
	void statical();
	void dynamical(move pv_mv);

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

	struct quiet_parameters
	{
		move hash;
		move prev1;
		move prev2;
		move counter;
		const killer_list* killer;
		const history* hist;
	} quiets{};

	score see_margin{ score::none };

private:
	// weighting captures & promotions

	uint32 mvv_lva(move mv) const;
	uint32 mvv_lva_promo(move mv) const;

	bool previous(move mv) const;

public:
	// main search

	sort(gen<md>& mvlist, move mv_tt, move mv_prev1, move mv_prev2, move counter, const killer_list& killer, const history& hist_tables)
		: list{ mvlist }, quiets{ mv_tt, mv_prev1, mv_prev2, counter, &killer, &hist_tables } {}

	void hash();
	void tactical_see();
	void killer();
	void quiet();
	void loosing();
	void deferred();

	// quiescence search

	sort(gen<md> &mvlist) : list{ mvlist } {}

	void tactical();
	void evasion();
};