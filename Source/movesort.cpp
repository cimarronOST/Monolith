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

#include "move.h"
#include "attack.h"
#include "eval.h"
#include "see.h"
#include "movesort.h"

namespace
{
	// basis bonus scores

	const struct bonus_score
	{
		uint64 pv     { 1ULL << 63 };
		uint64 capture{ 1ULL << 62 };
		uint64 killer { capture + 3 };
		uint64 base   { 1ULL << 14 };
	} bonus;

	// quiet move bonus depending on piece and square

	const int value[] { 1, 3, 3, 5, 9, 0 };

	const struct quiet_weight
	{
		int piece[6]{ 10, 8, 8, 4, 3, 1 };
		int sq[64]
		{
			0, 0, 0, 0, 0, 0, 0, 0,
			1, 2, 2, 2, 2, 2, 2, 1,
			1, 2, 4, 4, 4, 4, 2, 1,
			1, 2, 4, 6, 6, 4, 2, 1,
			1, 2, 4, 6, 6, 4, 2, 1,
			1, 2, 4, 4, 4, 4, 2, 1,
			1, 2, 2, 2, 2, 2, 2, 1,
			0, 0, 0, 0, 0, 0, 0, 0
		};
		
	} weight;
}

void sort_root::statical()
{
	// assigning a base value to all root node moves

	for (auto i{ 0 }; i < list.cnt.moves; ++i)
	{
		pos.new_move(list.move[i]);
		root[i].move = list.move[i];
		root[i].weight = bonus.base - eval::static_eval(pos) + (i < list.cnt.capture ? see::eval(list.pos, list.move[i]) : 0);
		root[i].check = !attack::check(pos, pos.turn, pos.pieces[KINGS] & pos.side[pos.turn]);

		pos.revert(list.pos);
	}
}

void sort_root::dynamical(uint32 pv_move)
{
	// reordering the root node moves at every iteration

	for (auto i{ 0 }; i < list.cnt.moves; ++i)
	{
		root[i].weight += root[i].nodes;
		root[i].weight >>= 1;
		root[i].skip = false;
		
		if (root[i].move == pv_move)
			root[i].weight = bonus.pv;
	}
	std::stable_sort(root, root + list.cnt.moves, [&](root_node a, root_node b) { return a.weight > b.weight; });
}

void sort_root::exclude_move(uint32 prev_move)
{
	// excluding the best move from the previous iteration from the search, used only in multiPV

	assert(list.in_list(prev_move));

	for (auto i{ 0 }; i < list.cnt.moves; ++i)
	{
		if (root[i].move == prev_move)
		{
			root[i].skip = true;
			break;
		}
	}
}

int sort::mvv_lva(uint32 move) const
{
	// returning victim-value * 100 - attacker-value for captures

	assert(move::victim(move) != NONE);

	return see::value[move::victim(move)] - value[move::piece(move)];
}

int sort::mvv_lva_promo(uint32 move) const
{
	// returning imaginary victim-value * 100 for promotions

	assert(move::flag(move) >= PROMO_KNIGHT);
	assert(see::value[NONE] == 0);

	return see::value[move::victim(move)] + see::value[move::flag(move) - 11] - 2 * see::value[PAWNS];
}

bool sort::is_loosing(board &pos, uint32 move) const
{
	// checking through SEE if <move> is loosing material

	assert(move::victim(move) != NONE);
	assert(move::piece(move) != NONE);

	auto piece{ move::piece(move) };

	if (piece == KINGS)
		return false;
	else if (value[move::victim(move)] >= value[piece])
		return false;
	else
		return see::eval(pos, move) < 0;
}

bool sort::is_double(uint32 move) const
{
	// checking if a quiet move has already been searched in a previous stage

	return move == quiets.hash_move
		|| move == quiets.killer->list[quiets.depth][0]
		|| move == quiets.killer->list[quiets.depth][1]
		|| move == quiets.counter;
}

void sort::hash()
{
	// weighting the hash move

	if (quiets.hash_move != NO_MOVE)
	{
		assert(list.cnt.moves == 1);
		assert(list.move[0] == quiets.hash_move);
		score[0] = bonus.pv;
	}
}

void sort::tactical_see()
{
	// weighting captures and promotions with MVV-LVA & SEE

	assert(list.cnt.loosing == 0);

	for (auto i{ 0 }; i < list.cnt.capture; ++i)
	{
		assert(!move::is_quiet(list.move[i]));
		if (list.move[i] == quiets.hash_move)
			score[i] = 0ULL;
		else if (is_loosing(list.pos, list.move[i]))
		{
			score[i] = 0ULL;
			list.move[lim::moves - ++list.cnt.loosing] = list.move[i];
		}
		else
			score[i] = bonus.capture + mvv_lva(list.move[i]);
	}

	// promotion MVV-LVA

	for (int i{ list.cnt.capture }; i < list.cnt.capture + list.cnt.promo; ++i)
	{
		assert(move::is_promo(list.move[i]));
		if (list.move[i] == quiets.hash_move)
			score[i] = 0ULL;
		else
			score[i] = bonus.capture + mvv_lva_promo(list.move[i]);
	}
}

void sort::killer()
{
	// weighting the two killer moves and the counter move

	assert(list.cnt.moves <= 3);
	for (auto i{ 0 }; i < list.cnt.moves; ++i)
	{
		assert(move::is_quiet(list.move[i]));
		if (list.move[i] == quiets.hash_move)
			score[i] = 0ULL;
		else
			score[i] = bonus.killer - i;
	}
}

void sort::quiet()
{
	// weighting quiet moves through history move heuristics & square- and piece-weights

	for (auto i{ list.cnt.capture + list.cnt.promo }; i < list.cnt.moves; ++i)
	{
		assert(move::is_quiet(list.move[i]));
		if (is_double(list.move[i]))
			score[i] = 0ULL;
		else
		{
			auto piece{ move::piece(list.move[i]) };
			auto sq1{ move::sq1(list.move[i]) };
			auto sq2{ move::sq2(list.move[i]) };

			score[i] = quiets.history->list[list.pos.turn][piece][sq2];
			score[i] -= weight.piece[piece] * weight.sq[sq1];
			score[i] += weight.piece[piece] * weight.sq[sq2];
		}
	}
}

void sort::loosing()
{
	// weighting loosing captures with MVV-LVA

	for (auto i{ 0 }; i < list.cnt.loosing; ++i)
	{
		assert(!move::is_quiet(list.move[i]));
		assert(list.move[i] != quiets.hash_move);
		score[i] = mvv_lva(list.move[i]);
	}
}

void sort::tactical()
{
	// weighting captures and promotions with only MVV-LVA in quiescence search

	for (auto i{ 0 }; i < list.cnt.capture; ++i)
		score[i] = bonus.capture + mvv_lva(list.move[i]);

	// promotion MVV-LVA

	for (auto i{ list.cnt.capture }; i < list.cnt.capture + list.cnt.promo; ++i)
		score[i] = bonus.capture + mvv_lva_promo(list.move[i]);
}

void sort::evasion()
{
	// weighting check evasions (only in quiescene search)

	for (auto i{ 0 }; i < list.cnt.moves; ++i)
	{
		assert(move::is_quiet(list.move[i]));
		score[i] = 1ULL;
	}
}

void sort::check()
{
	// weighting quiet checks (only in quiescence search)

	for (auto i{ 0 }; i < list.cnt.moves; ++i)
	{
		assert(move::is_quiet(list.move[i]));
		score[i] = 1ULL;
	}
}