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

#include <tuple>

#include "types.h"

// a collection of useful functions to keep track of the search score

namespace sc
{
	bool mate(score sc);
	bool draw(score sc);
	bool good_enough_mate(score sc);
	bool mate_distance_pruning(score& alpha, score& beta, depth stack_dt);
	bool tt_cutoff(bound bd, score sc, score alpha, score beta);
	std::tuple<score, bound> make_bounded(score best_sc, score old_alpha, score beta);

	// search scores for endgame table-bases have different ranges

	namespace tb
	{
		bool mate(score sc);
		bool draw(score sc);
		bool refinable(int64& root_weight, score sc);
	}
}
