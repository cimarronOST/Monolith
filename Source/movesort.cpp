/*
  Monolith 2 Copyright (C) 2017-2020 Jonas Mayr
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
#include "movesort.h"

template class sort<mode::legal>;
template class sort<mode::pseudolegal>;

void history::update_entry(int& entry, int weight)
{
	// updating a entry
	// a depth-depending weight is added, and additionally the entry is gravitated towards negativity
	// idea for the history gravity from Stockfish:
	// https://github.com/official-stockfish/Stockfish

	entry += 32 * weight - entry * std::abs(weight) / 128;
	if (std::abs(entry) >= max)
	{
		entry = std::min(entry,  max);
		entry = std::max(entry, -max);
	}
}

void history::set_main(move mv, int weight)
{
	// updating the main history table

	update_entry(main[mv.cl()][mv.pc()][mv.sq2()], weight);
}

void history::set_counter(move mv, move mv_prev, int weight)
{
	// updating the counter-move-history table

	if (mv_prev == move{})
		return;
	
	verify(mv_prev.cl() != mv.cl());
	update_entry(counter[mv_prev.pc()][mv_prev.sq2()][mv.pc()][mv.sq2()], weight);
}

void history::set_continuation(move mv, move mv_prev, int weight)
{
	// updating the continuation-history table

	if (mv_prev == move{})
		return;

	verify(mv_prev.cl() == mv.cl());
	update_entry(continuation[mv_prev.pc()][mv_prev.sq2()][mv.pc()][mv.sq2()], weight);
}

void history::update(move mv, move mv_prev1, move mv_prev2, const move_list& quiet_mv, int quiet_cnt, depth dt)
{
	// updating history table if a quiet move fails high

	verify(type::dt(dt) && dt >= 1);
	verify(quiet_cnt < quiet_mv.size());
	verify(quiet_mv[quiet_cnt - 1] == mv);

	int bonus{ std::min(dt * dt, 500) };
	int malus{ -dt };

	set_main(mv, bonus);
	set_counter(mv, mv_prev1, bonus);
	set_continuation(mv, mv_prev2, bonus);

	for (int i{}; i < quiet_cnt - 1; ++i)
	{
		set_main(quiet_mv[i], malus);
		set_counter(quiet_mv[i], mv_prev1, malus);
		set_continuation(quiet_mv[i], mv_prev2, malus);
	}
}

void history::clear()
{
	// clearing all tables

	for (auto&  cl : main)         for (auto&  pc : cl)  for (auto&  sq : pc)  sq = 0;
	for (auto& pc1 : counter)      for (auto& sq1 : pc1) for (auto& pc2 : sq1) for (auto& sq2 : pc2) sq2 = 0;
	for (auto& pc1 : continuation) for (auto& sq1 : pc1) for (auto& pc2 : sq1) for (auto& sq2 : pc2) sq2 = 0;
}

void history::sc::get(const history& hist, move mv, move mv_prev1, move mv_prev2)
{
	// retrieving the history scores from the tables

	verify(mv != move{});
	piece  pc{ mv.pc() };
	square sq2{ mv.sq2() };

	main = hist.main[mv.cl()][pc][sq2];
	counter = (mv_prev1 != move{} ? hist.counter[mv_prev1.pc()][mv_prev1.sq2()][pc][sq2] : 0);
	continuation = (mv_prev2 != move{} ? hist.continuation[mv_prev2.pc()][mv_prev2.sq2()][pc][sq2] : 0);
}

void rootsort::sort_moves()
{
	// sorting the root nodes

	std::stable_sort(root.begin(), root.begin() + list.cnt.mv, [&](root_node a, root_node b) { return a.weight > b.weight; });
}

void rootsort::statical()
{
	// assigning a base value to all root node moves through static evaluation (~12 Elo)

	kingpawn_hash hash(kingpawn_hash::allocate_none);
	for (int i{}; i < list.cnt.mv; ++i)
	{
		pos.new_move(list.mv[i]);
		root[i].mv = list.mv[i];
		root[i].nodes = 0LL;
		root[i].weight = int64(-eval::static_eval(pos, hash));
		root[i].check = pos.check();
		root[i].skip = false;
		pos = list.pos;
	}
}

void rootsort::dynamical(move pv_mv)
{
	// reordering the root node moves at the beginning of every new iteration

	static constexpr int64 pv_bonus{ 1LL << 62 };

	for (int i{}; i < list.cnt.mv; ++i)
	{
		// weighting all moves through the amount of child-nodes the search visited so far (~17 Elo)

		verify(root[i].nodes >= 0);
		root[i].weight += root[i].nodes;
		root[i].weight /= 2;
		root[i].skip = false;

		// giving the best move from the previous iteration a big bonus (~22 Elo)

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
	// including the previously from the search excluded PV moves, used only in multi-PV mode

	for (int i{}; i < list.cnt.mv; ++i)
		root[i].skip = false;
}

template<mode md> uint32 sort<md>::mvv_lva(move mv) const
{
	// victim-value in centipawn-units - attacker-value in pawn-units

	verify(mv.vc() != no_piece);
	static constexpr std::array<int, 6> pawn_units{ { 1, 3, 3, 5, 9, 0 } };

	return uint32(attack::value[mv.vc()] - score(pawn_units[mv.pc()]));
}

template<mode md> uint32 sort<md>::mvv_lva_promo(move mv) const
{
	// approximated values for promotions

	verify(mv.fl() >= promo_knight);
	verify(attack::value[no_piece] == score(0));

	return uint32(attack::value[mv.vc()] + attack::value[mv.promo_pc()] - attack::value[pawn] * 2);
}

template<mode md> bool sort<md>::previous(move mv) const
{
	// checking if a quiet move has already been searched in a previous stage

	return mv == quiets.hash
		|| mv == (*quiets.killer)[0]
		|| mv == (*quiets.killer)[1]
		|| mv == quiets.counter;
}

template void sort<mode::legal>::hash();
template void sort<mode::pseudolegal>::hash();

template<mode md> void sort<md>::hash()
{
	// weighting the hash move

	if (quiets.hash)
	{
		verify(list.cnt.mv == 1);
		verify(list.mv[0] == quiets.hash);
		sc[0] = 1;
	}
}

template void sort<mode::legal>::tactical_see();
template void sort<mode::pseudolegal>::tactical_see();

template<mode md> void sort<md>::tactical_see()
{
	// weighting captures with MVV-LVA

	verify(list.cnt.loosing == 0);
	verify(list.cnt.mv == list.cnt.capture + list.cnt.promo);

	for (int i{}; i < list.cnt.capture; ++i)
	{
		verify(list.mv[i].capture());
		if (list.mv[i] == quiets.hash)
		{
			sc[i] = 0;
			list.cnt.duplicate += 1;
		}
		else if (!attack::see_above(list.pos, list.mv[i], score(0)))
		{
			// deferring loosing captures to a later stage (~116 Elo)

			sc[i] = 0;
			list.mv[lim::moves - ++list.cnt.loosing] = list.mv[i];
		}
		else
		{
			// sorting all non-loosing captures with MVV-LVA (~23 ELo)

			sc[i] = mvv_lva(list.mv[i]);
		}
	}

	// weighting promotions with MVV-LVA (~0 Elo)

	for (int i{ list.cnt.capture }; i < list.cnt.capture + list.cnt.promo; ++i)
	{
		verify(list.mv[i].promo());
		if (list.mv[i] == quiets.hash)
		{
			sc[i] = 0;
			list.cnt.duplicate += 1;
		}
		else
			sc[i] = mvv_lva_promo(list.mv[i]);
	}
}

template void sort<mode::legal>::killer();
template void sort<mode::pseudolegal>::killer();

template<mode md> void sort<md>::killer()
{
	// weighting the two killer moves and the counter move

	verify(list.cnt.mv <= 3);
	for (int i{}; i < list.cnt.mv; ++i)
	{
		verify(list.mv[i].quiet());
		if (list.mv[i] == quiets.hash)
		{
			sc[i] = 0;
			list.cnt.duplicate += 1;
		}
		else
			sc[i] = list.cnt.mv - i;
	}
}

template void sort<mode::legal>::quiet();
template void sort<mode::pseudolegal>::quiet();

template<mode md> void sort<md>::quiet()
{
	// weighting quiet moves through history heuristic (~184 Elo)

	piece  pc_prev1{ quiets.prev1.pc() },  pc_prev2{ quiets.prev2.pc() };
	square sq_prev1{ quiets.prev1.sq2() }, sq_prev2{ quiets.prev2.sq2() };

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
			piece  pc{ list.mv[i].pc() };
			square sq{ list.mv[i].sq2() };
			sc[i]  = history::max * 3 + 1;
			sc[i] += (*quiets.hist).main[list.pos.cl][pc][sq];
			sc[i] += !quiets.prev1 ? 0 :      (*quiets.hist).counter[pc_prev1][sq_prev1][pc][sq];
			sc[i] += !quiets.prev2 ? 0 : (*quiets.hist).continuation[pc_prev2][sq_prev2][pc][sq];

			verify(list.pos.cl == list.mv[i].cl());
			verify(sc[i] >= 1 && sc[i] <= uint32(history::max) * 6 + 1);
		}
	}
}

template void sort<mode::legal>::loosing();
template void sort<mode::pseudolegal>::loosing();

template<mode md> void sort<md>::loosing()
{
	// weighting loosing captures with MVV-LVA (~8 Elo)

	verify(list.cnt.loosing == list.cnt.mv);
	for (int i{}; i < list.cnt.loosing; ++i)
	{
		verify(list.mv[i].capture() && !list.mv[i].promo());
		verify(list.mv[i] != quiets.hash);
		sc[i] = mvv_lva(list.mv[i]);
	}
}

template void sort<mode::legal>::deferred();
template void sort<mode::pseudolegal>::deferred();

template<mode md> void sort<md>::deferred()
{
	// taking care that the order of deferred moves is kept untouched

	for (int i{}; i < list.cnt.mv; ++i)
		sc[i] = 1;
}

template void sort<mode::legal>::tactical();
template void sort<mode::pseudolegal>::tactical();

template<mode md> void sort<md>::tactical()
{
	// weighting tactical moves in quiescence search (~17 Elo)
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

template void sort<mode::legal>::evasion();
template void sort<mode::pseudolegal>::evasion();

template<mode md> void sort<md>::evasion()
{
	// weighting check evasions (only in quiescence search)

	for (int i{}; i < list.cnt.mv; ++i)
	{
		verify(list.mv[i].quiet());
		sc[i] = 1;
	}
}