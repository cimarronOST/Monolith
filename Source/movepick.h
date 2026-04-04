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
#include "uci.h"
#include "move.h"
#include "movesort.h"
#include "movegen.h"
#include "types.h"
#include "board.h"

// orchestrating the generation and sorting of the root node move-list

class rootpick
{
private:
	gen<mode::LEGAL> list;
	rootsort sort;
	int mv_n{};

public:
	bool tb_pos{};
	int  mv_cnt()       const { return list.cnt.mv; };
	bool single_reply() const { return mv_cnt() == 1; };
	bool tb_win()       const { return (score)sort.root[0].weight == TB_WIN; };

	rootpick(const board& pos) : list{ pos }, sort{ list }
	{
		if (!uci::limit.searchmoves.empty())
			list.gen_searchmoves(uci::limit.searchmoves);
		else
			list.gen_all();

		sort.sort_static();
	}

	// adjusting the root move order

	void rearrange_list(move pv_mv, move multipv_mv);
	void sort_tb() { sort.sort_moves(); };

	// picking the next move from the list

	rootsort::root_node*  next() { return mv_n < mv_cnt() ? &sort.root[mv_n++] : nullptr; };
	rootsort::root_node* first() { mv_n = 0; return next(); };

	// reverting the move

	void revert(board& pos) const { pos = list.pos; }
};

// orchestrating the generation and weighting of the move-list, and picking moves from it

template<mode md> class movepick
{
public:
	gen<md> list;
	int hits{};

private:
	sort<md> weight;
	std::array<genstage, 5> st{};

	struct move_cnt
	{
		int cycles{ -1 };
		int max_cycles{};
		int attempts{};
		int mv{};
	} cnt;

	// generating and weighting the moves

	void gen_weight();

public:
	// alpha-beta search

	movepick(board& pos, move mv_tt, const sstack* stack, move counter, const history& hist)
		: list{ pos }, weight{ list, mv_tt, stack, counter, hist }
	{
		cnt.max_cycles = 5;
		st = { { genstage::HASH, genstage::WINNING, genstage::KILLER, genstage::QUIET, genstage::LOOSING } };
	}

	// quiescence search

	movepick(board &pos, bool in_check) : list{ pos }, weight{ list }
	{
		cnt.max_cycles = 1 + in_check;
		st = { { genstage::TACTICAL, genstage::EVASION } };
	}

public:
	void revert(board& pos) { pos = list.pos; hits -= 1; }

	// picking the highest weighted move
	// if there is none left, further generation and weighting is initialized

	move next();
};
