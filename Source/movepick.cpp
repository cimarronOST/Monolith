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


#include "movepick.h"

void movepick_root::rearrange_moves(uint32 pv_move, uint32 multipv_move)
{
	// resorting the root node moves or excluding already searched multipv move

	if (multipv_move == NO_MOVE)
		sort.dynamical(pv_move);
	else
		sort.exclude_move(multipv_move);
}

bool movepick_root::single_reply() const
{
	// checking if there is only a single legal move

	return list.cnt.moves == 1
		&& !engine::infinite
		&& engine::limit.moves.empty();
}

void movepick::gen_weight()
{
	// generating and weighting the moves of the current stage

	list.cnt.moves = list.cnt.capture = list.cnt.promo = 0;
	assert(cnt.cycles >= 0);

	switch (stage[cnt.cycles])
	{
	case HASH:
		cnt.moves = list.gen_hash(weight.quiets.hash_move);
		weight.hash();
		break;

	case WINNING:
		cnt.moves = list.gen_tactical<PROMO_ALL>();
		weight.tactical_see();
		break;

	case KILLER:
		cnt.moves = list.gen_killer(weight.quiets.killer->list, weight.quiets.depth, weight.quiets.counter);
		weight.killer();
		break;

	case QUIET:
		cnt.moves = list.gen_quiet();
		weight.quiet();
		break;

	case LOOSING:
		cnt.moves = list.gen_loosing();
		weight.loosing();
		break;

	case TACTICAL:
		cnt.moves = list.gen_tactical<PROMO_QUEEN>();
		weight.tactical();
		break;

	case CHECK:
		cnt.moves = list.gen_check();
		weight.check();
		break;

	case EVASION:
		cnt.moves = list.gen_quiet();
		weight.evasion();
		break;

	default:
		assert(false);
	}
	cnt.attempts = cnt.moves;
}

uint32 movepick::next()
{
	// cycling through generation stages

	while (cnt.attempts == 0)
	{
		cnt.cycles += 1;
		if (cnt.cycles >= cnt.max_cycles)
			return NO_MOVE;
		gen_weight();
	}

	// finding the highest score

	move_idx = -1;
	auto best_score{ 0ULL };

	for (auto i{ 0 }; i < cnt.moves; ++i)
	{
		if (weight.score[i] > best_score)
		{
			best_score = weight.score[i];
			move_idx = i;
		}
	}

	assert(cnt.attempts >= 1);
	assert(cnt.moves == list.cnt.moves);
	cnt.attempts -= 1;

	if (move_idx == -1)
	{
		assert(stage[cnt.cycles] != HASH);
		assert(cnt.attempts == 0
			|| stage[cnt.cycles] == WINNING
			|| stage[cnt.cycles] == QUIET
			|| engine::multipv > 1);

		cnt.attempts = 0;
		return next();
	}

	attempts += 1;
	weight.score[move_idx] = 0ULL;
	return list.move[move_idx];
}