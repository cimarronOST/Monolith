/*
  Monolith 0.2  Copyright (C) 2017 Jonas Mayr

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
#include "chronos.h"
#include "main.h"

namespace analysis
{
	void reset();

#ifdef DEBUG

	void summary(timer &time);
	void root_perft(pos &board, int depth_min, int depth_max);
	uint64 perft(pos &board, int depth);

#endif
}

namespace search
{
	uint16 id_frame(pos &board, chronos &chrono);
	void root_alphabeta(pos &board, uint16 pv[], int &best_score, int ply);
	int alphabeta(pos &board, int ply, int depth, int alpha, int beta);

	int qsearch(pos &board, int alpha, int beta);
};
