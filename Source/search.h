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

#include "movepick.h"
#include "movegen.h"
#include "position.h"
#include "chronos.h"
#include "main.h"

// analysis functions for debugging

namespace analysis
{
	void reset();
	void summary(chronometer &time);

	void root_perft(pos &board, int depth, const GEN_MODE mode);
	uint64 perft(pos &board, int depth, const GEN_MODE mode);
}

// actual search

namespace search
{
	uint32 id_frame(pos &board, chronos &chrono, uint32 &ponder);

	int root_alphabeta(pos &board, movepick &pick, uint32 pv[], int ply);
	int alphabeta(pos &board, int ply, int depth, int alpha, int beta);

	int qsearch(pos &board, int alpha, int beta);

	void stop_ponder();
};
