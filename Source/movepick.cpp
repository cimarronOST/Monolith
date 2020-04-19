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


#include "movepick.h"

template class movepick<mode::legal>;
template class movepick<mode::pseudolegal>;

void rootpick::rearrange_list(move pv_mv, move multipv_mv)
{
	// resorting the root node moves at every iteration & excluding already searched multi-PV moves,
	// but not resorting anything if the position has been found in the tablebases,
	// because in that case the move order is already perfect

	if (multipv_mv)
		sort.exclude_move(multipv_mv);
	else if (!tb_pos)
		sort.dynamical(pv_mv);
	else
		sort.include_moves();
}

template<mode md> void movepick<md>::gen_weight()
{
	// generating and weighting the moves of the current stage

	list.cnt.mv = list.cnt.capture = list.cnt.promo = list.cnt.duplicate = 0;
	verify(cnt.cycles >= 0);

	switch (st[cnt.cycles])
	{
	// main search stages

	case genstage::hash:
		cnt.mv = list.gen_hash(weight.quiets.hash);
		weight.hash();
		break;

	case genstage::winning:
		cnt.mv  = list.gen_capture();
		cnt.mv += list.gen_promo(stage::promo_all);
		weight.tactical_see();
		break;

	case genstage::killer:
		cnt.mv = list.gen_killer(*weight.quiets.killer, weight.quiets.counter);
		weight.killer();
		break;

	case genstage::quiet:
		cnt.mv = list.gen_quiet();
		weight.quiet();
		break;

	case genstage::loosing:
		cnt.mv = list.restore_loosing();
		weight.loosing();
		break;

	case genstage::deferred:
		cnt.mv = list.restore_deferred(*deferred_mv, *deferred_cnt);
		weight.deferred();
		break;

	// quiescence search stages

	case genstage::tactical:
		cnt.mv  = list.gen_capture();
		cnt.mv += list.gen_promo(stage::promo_queen);
		weight.tactical();
		break;

	case genstage::evasion:
		cnt.mv = list.gen_quiet();
		weight.evasion();
		break;

	default:
		verify(false);
	}
	cnt.attempts = cnt.mv;
}

template move movepick<mode::legal>::next();
template move movepick<mode::pseudolegal>::next();

template<mode md> move movepick<md>::next()
{
	// cycling through move-generation stages and picking the highest-scored moves

	while (cnt.attempts == 0)
	{
		cnt.cycles += 1;
		if (cnt.cycles >= cnt.max_cycles)
			return move{};

		gen_weight();
	}

	// finding the highest-scored move

	int best_idx{ -1 };
	uint32 best_score{};

	for (int i{}; i < cnt.mv; ++i)
	{
		if (weight.sc[i] > best_score)
		{
			best_score = weight.sc[i];
			best_idx = i;
		}
	}

	if (best_idx == -1)
	{
		// even though there are still moves in the list of the current stage that have not been selected,
		// next() will be moving on to the next generation stage, because hash-, killer- and counter-moves
		// are generated twice, and moves with negative SEE are being deferred to the 'loosing' stage
		// after their generation, so all of these can be skipped safely

		verify(cnt.attempts == list.cnt.duplicate + (st[cnt.cycles] == genstage::winning ? list.cnt.loosing : 0)
			|| uci::multipv > 1);
		cnt.attempts = 0;
		return next();
	}

	verify(cnt.attempts >= 1);
	verify(cnt.mv == list.cnt.mv);
	cnt.attempts -= 1;

	// a move has been found successfully

	hits += 1;
	weight.sc[best_idx] = 0ULL;
	return list.mv[best_idx];
}