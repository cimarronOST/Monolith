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


#include "evaluation.h"
#include "see.h"
#include "sort.h"

namespace
{
	const int value[]{ 1, 3, 3, 5, 9, 0, 1 };

	const uint64  max_score{ 1ULL << 63 };
	const uint64 capt_score{ 1ULL << 62 };
}

int sort::mvv_lva(uint32 move) const
{
	assert(to_victim(move) != NONE);

	return see::exact_value[to_victim(move)] - value[to_piece(move)];
}

int sort::mvv_lva_promo(uint32 move) const
{
	assert(to_flag(move) >= PROMO_KNIGHT);
	assert(see::exact_value[NONE] == 0);

	int victim{ see::exact_value[to_victim(move)] + see::exact_value[to_flag(move) - 11] };

	return victim - 2 * see::exact_value[PAWNS] - value[to_flag(move) - 11];
}

bool sort::loosing(pos &board, uint32 move) const
{
	assert(to_victim(move) != NONE);
	assert(to_piece(move) != NONE);

	auto piece{ to_piece(move) };

	if (piece == KINGS)
		return false;
	else if (value[to_victim(move)] >= value[piece])
		return false;
	else
		return see::eval(board, move) < 0;
}

void sort::hash(GEN_STAGE stage)
{
	if (list.hash_move != NO_MOVE)
	{
		uint32 *pos_list{ list.find(list.hash_move) };

		assert(stage != ALL);

		if (pos_list != list.movelist + list.cnt.moves)
			score[pos_list - list.movelist] = (stage == HASH ? max_score : 0ULL);
	}
}

void sort::tactical_see(GEN_STAGE stage)
{
	assert(list.cnt.loosing == 0);

	// capture mvv-lva & SEE

	for (int i{ 0 }; i < list.cnt.capture; ++i)
	{
		if (loosing(list.board, list.movelist[i]))
		{
			assert(stage != ALL);

			score[i] = 0ULL;
			list.movelist[lim::movegen - ++list.cnt.loosing] = list.movelist[i];
		}

		else
			score[i] = capt_score + mvv_lva(list.movelist[i]);
	}

	// promotion mvv-lva

	for (int i{ list.cnt.capture }; i < list.cnt.capture + list.cnt.promo; ++i)
		score[i] = capt_score + mvv_lva_promo(list.movelist[i]);
}

void sort::tactical()
{
	// capture mvv-lva

	for (int i{ 0 }; i < list.cnt.capture; ++i)
		score[i] = capt_score + mvv_lva(list.movelist[i]);

	// promotion mvv-lva

	for (int i{ list.cnt.capture }; i < list.cnt.capture + list.cnt.promo; ++i)
		score[i] = capt_score + mvv_lva_promo(list.movelist[i]);
}

void sort::quiet()
{
	// history heuristic

	for (int i{ list.cnt.capture + list.cnt.promo }; i < list.cnt.capture + list.cnt.promo + list.cnt.quiet; ++i)
	{
		assert(list.board.turn == to_turn(list.movelist[i]));
		score[i] = main.history->list[list.board.turn][to_piece(list.movelist[i])][to_sq2(list.movelist[i])];
	}

	// killer move heuristic

	for (int slot{ 0 }; slot < 2; ++slot)
	{
		auto piece{ to_piece(main.killer->list[main.depth][slot]) };

		if (piece != list.board.piece_sq[to_sq1(main.killer->list[main.depth][slot])])
			continue;

		for (int i{ list.cnt.capture + list.cnt.promo }; i < list.cnt.capture + list.cnt.promo + list.cnt.quiet; ++i)
		{
			if (list.movelist[i] == main.killer->list[main.depth][slot])
			{
				score[i] = capt_score + 2 - slot;
				break;
			}
		}
	}
}

void sort::loosing()
{
	for (int i{ 0 }; i < list.cnt.loosing; ++i)
	{
		score[i] = mvv_lva(list.movelist[i]);
	}
}

void sort::root_static(uint64 nodes[])
{
	pos saved{ list.board };

	// evaluation & SEE of captures

	for (int i{ 0 }; i < list.cnt.capture; ++i)
	{
		list.board.new_move(list.movelist[i]);
		score[i] = NO_SCORE - eval::static_eval(list.board) + see::eval(saved, list.movelist[i]);
		list.board = saved;
	}

	// evaluation of promotions and quiets

	for (int i{ list.cnt.capture }; i < list.cnt.moves; ++i)
	{
		list.board.new_move(list.movelist[i]);
		score[i] = NO_SCORE - eval::static_eval(list.board);
		list.board = saved;
	}

	// assigning base value for root node-count

	for (int i{ 0 }; i < list.cnt.moves; ++i)
		nodes[i] = score[i];
}

void sort::root_dynamic(uint64 nodes[], uint32 *pv_move)
{
	assert(*pv_move != NO_MOVE);

	// sorting by node count

	for (int i{ 0 }; i < list.cnt.moves; ++i)
	{
		score[i] = nodes[i];
		nodes[i] >>= 1; 
	}

	// pv-move

	score[pv_move - list.movelist] = max_score;
}
