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


#include <cmath>
#include <algorithm>
#include <tuple>

#include "main.h"
#include "types.h"
#include "uci.h"
#include "score.h"

bool sc::mate(score sc)
{
	// checking whether the score is a mate score

	verify(type::sc(sc));
    return std::abs(sc) >= LONGEST_MATE;
}

bool sc::draw(score sc)
{
	// checking whether the score is a draw score

	verify(type::sc(sc));
    return sc == DRAW;
}

bool sc::good_enough_mate(score sc)
{
	// checking whether the score is a good enough mate as defined by the UCI 'go mate' command

	verify(type::sc(sc));
    return MATE - sc <= uci::limit.mate * 2;
}

bool sc::mate_distance_pruning(score& alpha, score& beta, depth stack_dt)
{
	// pruning if mate-score bounds for alpha & beta are exceeded

    alpha = std::max(-MATE + score(stack_dt), alpha);
    beta = std::min(MATE - score(stack_dt + 1), beta);
	return alpha >= beta;
}

bool sc::tt_cutoff(bound bd, score sc, score alpha, score beta)
{
	// determining if conditions for a tt-cutoff are met

    return  bd == bound::EXACT
        || (bd == bound::LOWER && sc >= beta)
        || (bd == bound::UPPER && sc <= alpha);
}

std::tuple<score, bound> sc::make_bounded(score best_sc, score old_alpha, score beta)
{
	// determining the bound of the score and combining it in a tuple

    bound bd{ best_sc <= old_alpha ? bound::UPPER : (best_sc >= beta ? bound::LOWER : bound::EXACT) };
	return { best_sc, bd };
}

bool sc::tb::mate(score sc)
{
	// checking whether the score is a table-base win or loss

	verify(type::sc(sc));
    return std::abs(sc) >= TB_WIN - lim::dtz && std::abs(sc) <= TB_WIN + lim::dtz;
}

bool sc::tb::draw(score sc)
{
	// checking whether the score is a table-base draw score

	verify(type::sc(sc));
	return !mate(sc);
}

bool sc::tb::refinable(int64& root_weight, score sc)
{
	// checking whether the search score can be refined with the more accurate table-base score

	score dtz{ (score)root_weight };
	return (mate(dtz) && !sc::mate(sc)) || (draw(dtz) && !sc::draw(sc));
}
