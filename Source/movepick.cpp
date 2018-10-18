/*
  Monolith 1.0  Copyright (C) 2017-2018 Jonas Mayr

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

void rootpick::rearrange_moves(uint32 pv_move, uint32 multipv_move)
{
	// resorting the root node moves at every iteration & excluding already searched multi-PV moves,
	// but not resorting anything if the position has been found in the tablebases,
	// because in that case the move order is already perfect

	if (multipv_move != MOVE_NONE)
		sort.exclude_move(multipv_move);
	else if (!tb_pos)
		sort.dynamical(pv_move);
	else
		sort.include_moves();
}

bool rootpick::single_reply() const
{
	// checking if there is only one single legal move

	return list.moves == 1
		&& !uci::infinite
		&&  uci::limit.moves.empty();
}

void movepick::gen_weight()
{
	// generating and weighting the moves of the current stage

	list.moves = list.count.capture = list.count.promo = 0;
	assert(count.cycles >= 0);

	switch (stage[count.cycles])
	{
	case HASH:
		count.moves = list.gen_hash(weight.quiets.hash);
		weight.hash();
		break;

	case WINNING:
		count.moves = list.gen_tactical<PROMO_ALL>();
		weight.tactical_see();
		break;

	case KILLER:
		count.moves = list.gen_killer(weight.quiets.killer, weight.quiets.counter);
		weight.killer();
		break;

	case QUIET:
		count.moves = list.gen_quiet();
		weight.quiet();
		break;

	case LOOSING:
		count.moves = list.restore_loosing();
		weight.loosing();
		break;

	case DEFERRED:
		count.moves = list.restore_deferred(deferred_moves);
		weight.deferred();
		break;

	case TACTICAL:
		count.moves = list.gen_tactical<PROMO_QUEEN>();
		weight.tactical();
		break;

	case CHECK:
		count.moves = list.gen_check();
		weight.check();
		break;

	case EVASION:
		count.moves = list.gen_quiet();
		weight.evasion();
		break;

	default:
		assert(false);
	}
	count.attempts = count.moves;
}

void movepick::revert(board &pos)
{
	// reverting to the previous board state

	pos = list.pos;
	hits -= 1;
}

bool movepick::can_defer() const
{
	// making sure the movepick-object is not in the stage of regenerating deferred moves
	// this is checked to make sure that moves are only deferred once
	// single-threaded, this condition is always false (and therefore no move gets deferred)

	return uci::thread_count > 1 && stage[count.cycles] != DEFERRED;
}

uint32 movepick::next()
{
	// cycling through move-generation stages and picking the highest-scored moves

	while (count.attempts == 0)
	{
		count.cycles += 1;
		if (count.cycles >= count.max_cycles)
			return MOVE_NONE;

		gen_weight();
	}

	// finding the highest-scored move

	int best_idx{ -1 };
	int64 best_score{};

	for (int i{}; i < count.moves; ++i)
	{
		if (weight.score[i] > best_score)
		{
			best_score = weight.score[i];
			best_idx = i;
		}
	}

	assert(count.attempts >= 1);
	assert(count.moves == list.moves);
	count.attempts -= 1;

	if (best_idx == -1)
	{
		// even though there are still moves of the current stage that have not been selected in this stage,
		// next() will be moving on to the next generation stage
		// reason being that hash-, killer- and counter-moves are generated twice, and moves with negative SEE
		// are being deferred to the LOOSING stage after their generation, so these can be skipped safely

		assert(stage[count.cycles] != HASH);
		assert(count.attempts == 0
			|| stage[count.cycles] == WINNING
			|| stage[count.cycles] == QUIET
			|| uci::multipv > 1);

		count.attempts = 0;
		return next();
	}

	// a move has been found successfully

	hits += 1;
	weight.score[best_idx] = 0ULL;
	return list.move[best_idx];
}