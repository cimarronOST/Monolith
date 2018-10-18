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


#include "pawn.h"
#include "move.h"
#include "attack.h"
#include "eval.h"
#include "movesort.h"


namespace bonus
{
	// basis bonus scores

	constexpr int64 pv_root{ 1ULL << 62 };
	constexpr int64 pv     { 1ULL << 62 };
	constexpr int64 capture{ 1ULL << 61 };
	constexpr int64 killer { capture + 3 };
}

namespace value
{
	// basic piece values in rough pawn-units

	constexpr int pawn_units[]{ 1, 3, 3, 5, 9, 0 };
}

void sort_root::sort_moves()
{
	// sorting the root nodes

	std::stable_sort(root, root + list.moves, [&](root_node a, root_node b) { return a.weight > b.weight; });
}

void sort_root::statical()
{
	// assigning a base value to all root node moves through static evaluation + SEE, only called once

	pawn hash(false);
	for (int i{}; i < list.moves; ++i)
	{
		pos.new_move(list.move[i]);
		root[i].move   = list.move[i];
		root[i].weight = -eval::static_eval(pos, hash) + attack::see(list.pos, list.move[i]);
		root[i].check  = pos.check();
		pos.revert(list.pos);
	}
}

void sort_root::statical_tb()
{
	// sorting the root node moves if the root position has been found in the tablebases
	// the weights of the moves correspond to their dtz/wdl-scores

	sort_moves();
}

void sort_root::dynamical(uint32 pv_move)
{
	// reordering the root node moves at every iteration

	for (int i{}; i < list.moves; ++i)
	{
		root[i].weight += root[i].nodes;
		root[i].weight /= 2;
		root[i].skip    = false;
		
		if (root[i].move == pv_move)
			root[i].weight = bonus::pv_root;
	}
	sort_moves();
}

void sort_root::exclude_move(uint32 multipv_move)
{
	// making sure to exclude the best move from the previous iteration from the search, used only in multi-PV mode

	assert(list.find(multipv_move));
	for (int i{}; i < list.moves; ++i)
	{
		if (root[i].move == multipv_move)
		{
			root[i].skip = true;
			break;
		}
	}
}

void sort_root::include_moves()
{
	// including the previously from the search excluded PV moves, used only in multi-PV mode

	for (int i{ 0 }; i < list.moves; ++i)
		root[i].skip = false;
}

int sort::mvv_lva(uint32 move) const
{
	// returning victim-value * 100 - attacker-value for captures

	assert(move::victim(move) != NONE);

	return attack::value[move::victim(move)] - value::pawn_units[move::piece(move)];
}

int sort::mvv_lva_promo(uint32 move) const
{
	// returning imaginary victim-value * 100 for promotions

	assert(move::flag(move) >= PROMO_KNIGHT);
	assert(attack::value[NONE] == 0);

	return attack::value[move::victim(move)] + attack::value[move::promo_piece(move)] - 2 * attack::value[PAWNS];
}

bool sort::loosing(board &pos, uint32 move) const
{
	// checking through SEE if the move leads to material loss

	assert(move::victim(move) != NONE);
	assert(move::piece(move)  != NONE);

	auto piece{ move::piece(move) };

	if (piece == KINGS)
		return false;
	else if (value::pawn_units[move::victim(move)] >= value::pawn_units[piece])
		return false;
	else
		return attack::see(pos, move) < 0;
}

bool sort::previous(uint32 move) const
{
	// checking if a quiet move has already been searched in a previous stage

	return move == quiets.hash
		|| move == quiets.killer[0]
		|| move == quiets.killer[1]
		|| move == quiets.counter;
}

void sort::hash()
{
	// weighting the hash move

	if (quiets.hash != MOVE_NONE)
	{
		assert(list.moves == 1);
		assert(list.move[0] == quiets.hash);
		score[0] = bonus::pv;
	}
}

void sort::tactical_see()
{
	// weighting captures with MVV-LVA & SEE

	assert(list.count.loosing == 0);

	for (int i{}; i < list.count.capture; ++i)
	{
		assert(!move::quiet(list.move[i]));
		if (list.move[i] == quiets.hash)
			score[i] = 0LL;
		else if (loosing(list.pos, list.move[i]))
		{
			score[i] = 0LL;
			list.move[lim::moves - ++list.count.loosing] = list.move[i];
		}
		else
			score[i] = bonus::capture + mvv_lva(list.move[i]);
	}

	// weighting promotions with MVV-LVA

	for (int i{ list.count.capture }; i < list.count.capture + list.count.promo; ++i)
	{
		assert(move::promo(list.move[i]));
		if (list.move[i] == quiets.hash)
			score[i] = 0LL;
		else
			score[i] = bonus::capture + mvv_lva_promo(list.move[i]);
	}
}

void sort::killer()
{
	// weighting the two killer moves and the counter move

	assert(list.moves <= 3);
	for (int i{}; i < list.moves; ++i)
	{
		assert(move::quiet(list.move[i]));
		if (list.move[i] == quiets.hash)
			score[i] = 0LL;
		else
			score[i] = bonus::killer - i;
	}
}

void sort::quiet()
{
	// weighting quiet moves through history-count heuristic

	for (int i{ list.count.capture + list.count.promo }; i < list.moves; ++i)
	{
		assert(move::quiet(list.move[i]));
		if (previous(list.move[i]))
			score[i] = 0LL;
		else
		{
			score[i] = history_max + quiets.history[list.pos.turn][move::piece(list.move[i])][move::sq2(list.move[i])];
			assert(list.pos.turn == move::turn(list.move[i]));
			assert(score[i] > 0LL);
		}
	}
}

void sort::loosing()
{
	// weighting loosing captures with MVV-LVA

	for (int i{}; i < list.count.loosing; ++i)
	{
		assert(move::capture(list.move[i]) && !move::promo(list.move[i]));
		assert(list.move[i] != quiets.hash);
		score[i] = mvv_lva(list.move[i]);
	}
}

void sort::deferred()
{
	// weighting deferred moves, i.e. taking care that their order is kept untouched

	for (int i{}; i < list.moves; ++i)
		score[i] = 1ULL;
}

void sort::tactical()
{
	// weighting captures and promotions with only MVV-LVA in quiescence search

	for (int i{}; i < list.count.capture; ++i)
		score[i] = bonus::capture + mvv_lva(list.move[i]);

	// promotion MVV-LVA

	for (int i{ list.count.capture }; i < list.count.capture + list.count.promo; ++i)
		score[i] = bonus::capture + mvv_lva_promo(list.move[i]);
}

void sort::evasion()
{
	// weighting check evasions (only in quiescence search)

	for (int i{}; i < list.moves; ++i)
	{
		assert(move::quiet(list.move[i]));
		score[i] = 1LL;
	}
}

void sort::check()
{
	// weighting quiet checks (only in quiescence search)

	for (int i{}; i < list.moves; ++i)
	{
		assert(move::quiet(list.move[i]));
		score[i] = 1LL;
	}
}