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


#include "movepick.h"

void movepick::rearrange_root(uint64 nodes[], uint32 *pv_move)
{
	cnt.attempts = cnt.moves;
	weight.root_dynamic(nodes, pv_move);
}

void movepick::gen_weight()
{
	list.cnt.moves = list.cnt.hash = list.cnt.capture = list.cnt.promo = list.cnt.quiet = 0;

	switch (stage[cnt.cycles])
	{
	case ALL:
		cnt.moves = list.gen_tactical() + list.gen_quiet();
		weight.tactical_see(ALL);
		weight.quiet();
		weight.hash(ALL);
		break;

	case HASH:
		cnt.moves = list.gen_hash();
		weight.hash(HASH);
		break;

	case TACTICAL:
		cnt.moves = list.gen_tactical();
		weight.tactical();
		break;

	case WINNING:
		cnt.moves = list.gen_tactical();
		weight.tactical_see(WINNING);
		weight.hash(WINNING);
		break;

	case LOOSING:
		cnt.moves = list.gen_loosing();
		weight.loosing();
		weight.hash(LOOSING);
		break;

	case QUIET:
		cnt.moves = list.gen_quiet();
		weight.quiet();
		weight.hash(QUIET);
		break;

	default:
		assert(false);
	}

	cnt.attempts = cnt.moves;
}

uint32 *movepick::next()
{
	// cycling through generation stages

	while (cnt.attempts == 0)
	{
		cnt.cycles += 1;
		if (cnt.cycles >= cnt.max_cycles)
			return nullptr;

		gen_weight();
	}

	// finding the highest score

	int best_idx{ -1 };
	uint64 best_score{ 0 };

	for (int i{ 0 }; i < cnt.moves; ++i)
	{
		if (weight.score[i] > best_score)
		{
			best_score = weight.score[i];
			best_idx = i;
		}
	}

	assert(cnt.attempts >= 1);
	assert(cnt.moves == list.cnt.moves);

	cnt.attempts -= 1;

	// hash move and loosing moves might have gotten hidden

	if (best_idx == -1)
	{
		assert(stage[cnt.cycles] != HASH);
		assert(cnt.attempts == 0 || stage[cnt.cycles] == WINNING);

		return next();
	}

	weight.score[best_idx] = 0ULL;

	return &list.movelist[best_idx];
}
