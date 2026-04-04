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


#include "main.h"
#include "types.h"
#include "uci.h"
#include "movepick.h"
#include "move.h"

template class movepick<mode::LEGAL>;
template class movepick<mode::PSEUDOLEGAL>;

void rootpick::rearrange_list(move pv_mv, move multipv_mv)
{
	// sorting the root node moves at every iteration & excluding already searched multi-PV moves,
	// not sorting anything if the position has been found in the table-bases,
	// because in that case the move order is already perfect

	if (multipv_mv)
		sort.exclude_move(multipv_mv);
	else if (!tb_pos)
		sort.sort_dynamic(pv_mv);
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

	case genstage::HASH:
		cnt.mv = list.gen_hash(weight.node.hash);
		weight.hash();
		break;

	case genstage::WINNING:
		cnt.mv  = list.gen_capture();
		cnt.mv += list.gen_promo(stage::PROMO_ALL);
		weight.tactical_see();
		break;

	case genstage::KILLER:
		cnt.mv = list.gen_killer(*weight.node.killer, weight.node.counter);
		weight.killer();
		break;

	case genstage::QUIET:
		cnt.mv = list.gen_quiet();
		weight.quiet();
		break;

	case genstage::LOOSING:
		cnt.mv = list.restore_loosing();
		weight.loosing();
		break;

		// quiescence search stages

	case genstage::TACTICAL:
		cnt.mv  = list.gen_capture();
		cnt.mv += list.gen_promo(stage::PROMO_QUEEN);
		weight.tactical();
		break;

	case genstage::EVASION:
		cnt.mv = list.gen_quiet();
		weight.evasion();
		break;

	default:
		verify(false);
	}
	cnt.attempts = cnt.mv;
}

template move movepick<mode::LEGAL>::next();
template move movepick<mode::PSEUDOLEGAL>::next();
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

		verify(cnt.attempts == list.cnt.duplicate + (st[cnt.cycles] == genstage::WINNING ? list.cnt.loosing : 0)
			|| uci::multipv > 1);
		cnt.attempts = 0;
		return next();
	}

	verify(cnt.attempts >= 1);
	verify(cnt.mv == list.cnt.mv);

	cnt.attempts -= 1;
	hits += 1;
	weight.sc[best_idx] = 0ULL;
	return list.mv[best_idx];
}