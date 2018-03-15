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

#include <vector>

#include "stream.h"
#include "movepick.h"
#include "position.h"
#include "main.h"

// analysis functions for debugging

namespace analysis
{
	void reset();
	void summary();

	// doing a move-generator performance and correctness test

	void root_perft(board &pos, int depth, const gen_mode mode);
	uint64 perft(board &pos, int depth, const gen_mode mode);
}

// actual search

namespace search
{
	// uncluttering things with the search stack

	struct search_stack
	{
		int depth;
		uint32 move;
		struct tt_stack
		{
			uint32 move;
			int score;
			int bound;
		} tt;
		struct null_copy
		{
			uint64 ep;
			uint16 capture;
		} copy;
		bool no_pruning;
	};

	void init_stack(search_stack *stack);

	// bundling the principal varition

	struct variation
	{
		uint32 move[lim::depth];
		int mindepth;
		int maxdepth;
		int seldepth;
		int score;
	};

	void init_multipv(std::vector<variation> &multipv);

	// search functions hierarchy

	uint32 id_frame(board &pos, uint64 &movetime, uint32 &ponder);

	int aspiration(board &pos, search_stack *stack, movepick_root &pick, variation &pv, int depth, int multipv);
	int root_alphabeta(board &pos, search_stack *stack, movepick_root &pick, variation &pv, int depth, int multipv, int alpha, int beta);
	int alphabeta(board &pos, search_stack *stack, int depth, int alpha, int beta);
	int   qsearch(board &pos, search_stack *stack, int depth, int alpha, int beta);
};