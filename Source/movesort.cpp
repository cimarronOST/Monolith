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


#include <array>
#include <algorithm>

#include "main.h"
#include "types.h"
#include "attack.h"
#include "eval.h"
#include "history.h"
#include "move.h"
#include "movesort.h"

void rootsort::sort_moves()
{
	// sorting the root nodes

	std::stable_sort(root.begin(), root.begin() + list.cnt.mv, [&](root_node a, root_node b) { return a.weight > b.weight; });
}

void rootsort::sort_static()
{
	// assigning a base value to all root node moves through static evaluation (~10 Elo)

	kingpawn_hash hash(kingpawn_hash::ALLOCATE_NONE);
	for (int i{}; i < list.cnt.mv; ++i)
	{
		pos.new_move(list.mv[i]);
		root[i].nodes = 0LL;
		root[i].weight = int64(-eval::static_eval(pos, hash));
		root[i].mv = list.mv[i];
		root[i].check = pos.check();
		root[i].skip = false;
		pos = list.pos;
	}
}

void rootsort::sort_dynamic(move pv_mv)
{
	// reordering the root node moves at the beginning of every new iteration

	static constexpr int64 pv_bonus{ 1LL << 62 };

	for (int i{}; i < list.cnt.mv; ++i)
	{
		// weighting all moves through the amount of child-nodes the search visited so far (~25 Elo)

		verify(root[i].nodes >= 0);
		root[i].weight += root[i].nodes;
		root[i].weight /= 2;
		root[i].skip = false;

		// giving the best move from the previous iteration a big bonus (~5 Elo)

		if (root[i].mv == pv_mv)
			root[i].weight = pv_bonus;
	}
	sort_moves();
}

void rootsort::exclude_move(move multipv_mv)
{
	// making sure to exclude the best move from the previous iteration from the search, used only in multi-PV mode

	for (int i{}; i < list.cnt.mv; ++i)
	{
		if (root[i].mv == multipv_mv)
		{
			root[i].skip = true;
			return;
		}
	}
	verify(false);
}

void rootsort::include_moves()
{
	// including the PV moves that were excluded from the search previously, used only in multi-PV mode

	for (int i{}; i < list.cnt.mv; ++i)
		root[i].skip = false;
}

template<mode md> uint32 sort<md>::mvv_lva(move mv) const
{
	// value of victim in centipawns minus value of attacker in pawn-units

    verify(mv.vc() != NO_PIECE);
	static constexpr std::array<int, 6> pawn_units{ { 1, 3, 3, 5, 9, 0 } };

	return uint32(attack::value[mv.vc()] - score(pawn_units[mv.pc()]));
}

template<mode md> uint32 sort<md>::mvv_lva_promo(move mv) const
{
	// approximated values for promotions

    verify(mv.fl() >= PROMO_KNIGHT);
    verify(attack::value[NO_PIECE] == score(0));

    return uint32(attack::value[mv.vc()] + attack::value[mv.promo_pc()] - attack::value[PAWN] * 2);
}

template<mode md> bool sort<md>::previous(move mv) const
{
	// checking if a quiet move has already been searched in a previous stage

	return mv == node.hash
		|| mv == node.killer->mv[0]
		|| mv == node.killer->mv[1]
		|| mv == node.counter;
}

template void sort<mode::LEGAL>::hash();
template void sort<mode::PSEUDOLEGAL>::hash();
template<mode md> void sort<md>::hash()
{
	// weighting the hash move

	if (node.hash)
	{
		verify(list.cnt.mv == 1);
		verify(list.mv[0] == node.hash);
		sc[0] = 1;
	}
}

template void sort<mode::LEGAL>::tactical_see();
template void sort<mode::PSEUDOLEGAL>::tactical_see();
template<mode md> void sort<md>::tactical_see()
{
	// weighting captures with capture history

	verify(list.cnt.loosing == 0);
	verify(list.cnt.mv == list.cnt.capture + list.cnt.promo);

	for (int i{}; i < list.cnt.capture; ++i)
	{
		verify(list.mv[i].capture());
		if (list.mv[i] == node.hash)
		{
			sc[i] = 0;
			list.cnt.duplicate += 1;
		}
		else if (!attack::see_above(list.pos, list.mv[i], score(0)))
		{
			// deferring loosing captures to a later stage (~200 Elo)

			sc[i] = 0;
			list.mv[lim::moves - ++list.cnt.loosing] = list.mv[i];
		}
		else
		{
			// sorting all non-loosing captures with capture history (~20 Elo)

			sc[i] = history::max * 2
				+ (*node.hist).capture[list.pos.cl][list.mv[i].pc()][list.mv[i].sq2()][list.mv[i].vc()];
		}
	}

	// weighting capture promotions with MVV-LVA (~10 Elo)

	for (int i{ list.cnt.capture }; i < list.cnt.capture + list.cnt.promo; ++i)
	{
		verify(list.mv[i].promo());
		if (list.mv[i] == node.hash)
		{
			sc[i] = 0;
			list.cnt.duplicate += 1;
		}
		else
			sc[i] = mvv_lva_promo(list.mv[i]);
	}
}

template void sort<mode::LEGAL>::killer();
template void sort<mode::PSEUDOLEGAL>::killer();
template<mode md> void sort<md>::killer()
{
	// weighting the two killer moves (~15 Elo) and the counter move (~10 Elo)

	verify(list.cnt.mv <= 3);
	for (int i{}; i < list.cnt.mv; ++i)
	{
		verify(list.mv[i].quiet());
		if (list.mv[i] == node.hash)
		{
			sc[i] = 0;
			list.cnt.duplicate += 1;
		}
		else
			sc[i] = list.cnt.mv - i;
	}
}

template void sort<mode::LEGAL>::quiet();
template void sort<mode::PSEUDOLEGAL>::quiet();
template<mode md> void sort<md>::quiet()
{
	// weighting quiet moves through history heuristic (~210 Elo)

	move mv_1{ (node.stack - 1)->mv };
	move mv_2{ (node.stack - 2)->mv };
    piece  pc_1{ mv_1.pc()  }, pc_2{ mv_2.pc()  };
	square sq_1{ mv_1.sq2() }, sq_2{ mv_2.sq2() };

	for (int i{}; i < list.cnt.mv; ++i)
	{
		verify(list.mv[i].quiet());
		if (previous(list.mv[i]))
		{
			sc[i] = 0;
			list.cnt.duplicate += 1;
		}
		else
		{
			piece  pc { list.mv[i].pc()  };
			square sq1{ list.mv[i].sq1() };
			square sq { list.mv[i].sq2() };
			sc[i]  = history::max * 4;
			sc[i] += (*node.hist).main[list.pos.cl][sq1][sq];
			sc[i] += mv_1 ? (*node.hist).continuation[pc_1][sq_1][pc][sq][0] : 0;
			sc[i] += mv_2 ? (*node.hist).continuation[pc_2][sq_2][pc][sq][1] : 0;

			verify(list.pos.cl == list.mv[i].cl());
		}
	}
}

template void sort<mode::LEGAL>::loosing();
template void sort<mode::PSEUDOLEGAL>::loosing();
template<mode md> void sort<md>::loosing()
{
	// weighting loosing captures with capture history (~10 Elo)

	verify(list.cnt.loosing == list.cnt.mv);
	for (int i{}; i < list.cnt.loosing; ++i)
	{
		verify(list.mv[i].capture() && !list.mv[i].promo());
		verify(list.mv[i] != node.hash);
		sc[i] = history::max * 2
			+ (*node.hist).capture[list.pos.cl][list.mv[i].pc()][list.mv[i].sq2()][list.mv[i].vc()];
	}
}

template void sort<mode::LEGAL>::tactical();
template void sort<mode::PSEUDOLEGAL>::tactical();
template<mode md> void sort<md>::tactical()
{
	// weighting tactical moves in quiescence search (~5 Elo)
	// captures with MVV-LVA

	verify(list.cnt.capture + list.cnt.promo == list.cnt.mv);

	for (int i{}; i < list.cnt.capture; ++i)
	{
		verify(list.mv[i].capture());
		sc[i] = mvv_lva(list.mv[i]);
	}

	// promotions with MVV-LVA

	for (int i{ list.cnt.capture }; i < list.cnt.capture + list.cnt.promo; ++i)
	{
		verify(list.mv[i].promo());
		sc[i] = mvv_lva_promo(list.mv[i]);
	}
}

template void sort<mode::LEGAL>::evasion();
template void sort<mode::PSEUDOLEGAL>::evasion();
template<mode md> void sort<md>::evasion()
{
	// weighting check evasions (only in quiescence search)

	for (int i{}; i < list.cnt.mv; ++i)
	{
		verify(list.mv[i].quiet());
		sc[i] = 1;
	}
}