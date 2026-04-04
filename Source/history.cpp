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


#include <algorithm>
#include <cmath>

#include "main.h"
#include "types.h"
#include "move.h"
#include "board.h"
#include "history.h"

void history::update_entry(int& entry, int weight)
{
	// updating a history entry, the weight is depth dependent, the entry is then gravitated

	entry += (1 << w.hist_base) * weight - entry * std::abs(weight) / (1 << w.hist_gravity);
	entry = std::clamp(entry, -(int)max, (int)max);
}

void history::update_corr(int& entry, int weight)
{
	// updating a correction history entry, the weight is depth and score dependent

	entry += (1 << w.corr_base) * weight - entry * std::abs(weight) / (1 << w.corr_gravity);
}

void history::update_quiet(move mv, const sstack* ss, int cnt, int bonus, int malus)
{
	// updating the quiet move history tables, which are the main- and continuation-tables

	verify(ss->quiet_mv[cnt - 1] == mv);

	move cont1{ (ss - 1)->mv };
	move cont2{ (ss - 2)->mv };
	auto  pc{ mv.pc() };
	auto sq2{ mv.sq2() };
	auto cont1_pc { cont1.pc()  }, cont2_pc { cont2.pc()  };
	auto cont1_sq2{ cont1.sq2() }, cont2_sq2{ cont2.sq2() };

	update_entry(main[mv.cl()][mv.sq1()][sq2], bonus);
	if (cont1) update_entry(continuation[cont1_pc][cont1_sq2][pc][sq2][0], bonus);
	if (cont2) update_entry(continuation[cont2_pc][cont2_sq2][pc][sq2][1], bonus);

	for (int i{}; i < cnt - 1; ++i)
	{
		verify(ss->quiet_mv[i] != mv);
		pc   = ss->quiet_mv[i].pc();
		sq2  = ss->quiet_mv[i].sq2();

		update_entry(main[ss->quiet_mv[i].cl()][ss->quiet_mv[i].sq1()][sq2], malus);
		if (cont1) update_entry(continuation[cont1_pc][cont1_sq2][pc][sq2][0], malus);
		if (cont2) update_entry(continuation[cont2_pc][cont2_sq2][pc][sq2][1], malus);
	}
}

void history::update_capture(move mv, const sstack* ss, int cnt, int bonus, int malus)
{
	// updating the capture move history tables

	verify(ss->capture_mv[cnt - 1] == mv);
	update_entry(capture[mv.cl()][mv.pc()][mv.sq2()][mv.vc()], bonus);
	for (int i{}; i < cnt - 1; ++i)
	{
		auto mv_pre{ ss->capture_mv[i] };
		verify(mv_pre != mv);
		update_entry(capture[mv_pre.cl()][mv_pre.pc()][mv_pre.sq2()][mv_pre.vc()], malus);
	}
}

int history::idx_corr(key64& key)
{
	// indexing the correction history tables

	return key & (corr_size - 1);
}

void history::update(move mv, const sstack* ss, int quiet_cnt, int capture_cnt, depth dt)
{
	// updating history tables if alpha is raised

	verify(type::dt(dt) && dt >= 1);

	int bonus{ std::min(w.base_bonus + dt * dt, 500) };
	int malus{ w.base_malus - dt };

	if (mv.quiet())
		update_quiet(mv, ss, quiet_cnt, bonus, malus);
	else if (mv.capture())
		update_capture(mv, ss, capture_cnt, bonus, malus);
}

void history::update_corr(board& pos, sstack* ss, score best_sc, score eval, depth dt)
{
	// updating the correction history tables

	piece  pc{ (ss - 1)->mv.pc()  };
	square sq{ (ss - 1)->mv.sq2() };
	score  bonus{ std::clamp((best_sc - eval) * dt / (1 << w.corr_bonus), score(-256), score(256))};

	update_corr(corr_pawn[ pos.cl][idx_corr(pos.key.pawn)],  bonus);
	update_corr(corr_minor[pos.cl][idx_corr(pos.key.minor)], bonus);
	update_corr(corr_major[pos.cl][idx_corr(pos.key.major)], bonus);
	update_corr(corr_nonpawn[WHITE][pos.cl][idx_corr(pos.key.nonpawn[WHITE])], bonus);
	update_corr(corr_nonpawn[BLACK][pos.cl][idx_corr(pos.key.nonpawn[BLACK])], bonus);

	if ((ss - 2)->mv) update_corr((*(ss - 2)->cont_mv)[pc][sq], bonus);
	if ((ss - 3)->mv) update_corr((*(ss - 3)->cont_mv)[pc][sq], bonus);
	if ((ss - 4)->mv) update_corr((*(ss - 4)->cont_mv)[pc][sq], bonus);
	if ((ss - 5)->mv) update_corr((*(ss - 5)->cont_mv)[pc][sq], bonus);
}

score history::correct_sc(board& pos, sstack* ss, score sc)
{
	// probing the correction history tables and correcting the score

	if (sc  == score::NONE)
		return score::NONE;

	verify(ss->dt >= 1);
	piece  pc{ (ss - 1)->mv.pc()  };
	square sq{ (ss - 1)->mv.sq2() };
	int correction
	{
		  corr_pawn[ pos.cl][idx_corr(pos.key.pawn)]  * w.corr_mult[0] / 128
		+ corr_minor[pos.cl][idx_corr(pos.key.minor)] * w.corr_mult[1] / 128
		+ corr_major[pos.cl][idx_corr(pos.key.major)] * w.corr_mult[2] / 128
		+ corr_nonpawn[WHITE][pos.cl][idx_corr(pos.key.nonpawn[WHITE])] * w.corr_mult[3] / 128
		+ corr_nonpawn[BLACK][pos.cl][idx_corr(pos.key.nonpawn[BLACK])] * w.corr_mult[4] / 128
		+ (ss->dt >= 2 ? (*(ss - 2)->cont_mv)[pc][sq] * w.corr_mult[5] / 128 : 0)
		+ (ss->dt >= 3 ? (*(ss - 3)->cont_mv)[pc][sq] * w.corr_mult[6] / 128 : 0)
		+ (ss->dt >= 4 ? (*(ss - 4)->cont_mv)[pc][sq] * w.corr_mult[7] / 128 : 0)
		+ (ss->dt >= 5 ? (*(ss - 5)->cont_mv)[pc][sq] * w.corr_mult[8] / 128 : 0)
	};
	
	return score(std::clamp(sc + correction, TB_LOSS + 1, TB_WIN - 1));
}

void history::clear()
{
	// clearing all tables

	for (auto&  cl : main)         for (auto& sq1 :  cl) for (auto& sq2 : sq1) sq2 = 0;
	for (auto&  sd : capture)      for (auto& pc1 :  sd) for (auto&  sq : pc1) for (auto& pc2 : sq) pc2 = 0;
	for (auto& pc1 : continuation) for (auto& sq1 : pc1) for (auto& pc2 : sq1) for (auto& sq2 : pc2) for (auto& idx : sq2) idx = 0;
	for (auto&  cl : corr_pawn)    for (auto& idx :  cl) idx = 0;
	for (auto&  cl : corr_minor)   for (auto& idx :  cl) idx = 0;
	for (auto&  cl : corr_major)   for (auto& idx :  cl) idx = 0;
	for (auto&  cl : corr_nonpawn) for (auto& stm :  cl) for (auto& idx : stm) idx = 0;
	for (auto& pc1 : corr_cont)    for (auto& sq1 : pc1) for (auto& pc2 : sq1) for (auto& sq2 : pc2) sq2 = 0;
}

int history::sc::all() const
{
	// summing up all individual quiet history scores

	return main + cont1 + cont2;
}

void history::sc::get(const history& hist, move mv, const sstack* ss)
{
	// retrieving the quiet history scores from the tables

	verify(mv != move{});
	piece   pc{ mv.pc()  };
	square sq2{ mv.sq2() };

	main  = hist.main[mv.cl()][mv.sq1()][sq2];
	cont1 = (ss - 1)->mv ? hist.continuation[(ss - 1)->mv.pc()][(ss - 1)->mv.sq2()][pc][sq2][0] : 0;
	cont2 = (ss - 2)->mv ? hist.continuation[(ss - 2)->mv.pc()][(ss - 2)->mv.sq2()][pc][sq2][1] : 0;
}