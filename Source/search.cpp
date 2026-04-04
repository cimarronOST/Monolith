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
#include <mutex>
#include <chrono>
#include <array>

#include "main.h"
#include "types.h"
#include "score.h"
#include "history.h"
#include "zobrist.h"
#include "misc.h"
#include "attack.h"
#include "syzygy.h"
#include "movepick.h"
#include "movegen.h"
#include "move.h"
#include "trans.h"
#include "eval.h"
#include "uci.h"
#include "thread.h"
#include "board.h"
#include "search.h"

namespace
{
	bool abort(const chronometer& chrono, const std::array<move, lim::dt>& pv_mv, const rootpick& pick, depth dt, score sc)
	{
		// checking the criteria for early search abortion

		verify(type::dt(dt));
		verify(type::sc(sc));
		milliseconds time{ chrono.elapsed() };

		// special cases are UCI commands 'go infinite', 'go ponder', 'go mate' & 'go movetime'

		if (uci::infinite.load(std::memory_order::relaxed))
			return false;
		if (sc::good_enough_mate(sc))
			return true;
		if (chrono.movetime.fixed())
			return time >= chrono.movetime.target;

		// otherwise making sure that the search abortion is not delayed unnecessarily (~15 Elo)

		return  time > chrono.movetime.target / 8 * 5
			|| (time > chrono.movetime.target / 4 && dt > 8 && !pv_mv[dt - 8])
			|| (time > chrono.movetime.target / 8 && sc > LONGEST_MATE)
			|| (time > chrono.movetime.target / 8 && pick.tb_pos)
			|| (time > chrono.movetime.target / 32 && pick.single_reply());
	}
}

namespace null
{
	static void make_move(board& pos, sstack* stack, sthread& thread)
	{
		// doing a "null move"

		stack->mv = move{};
		stack->cont_mv = &thread.hist.corr_cont[0][NO_SQUARE];
		stack->null_mv = sstack::null_move{};
		pos.null_move(stack->null_mv.ep, stack->null_mv.sq);
		(stack + 1)->pruning = false;
	}

	static void revert_move(board& pos, sstack* stack)
	{
		// reverting the "null move"

		pos.revert_null_move(stack->null_mv.ep, stack->null_mv.sq);
		(stack + 1)->pruning = true;
	}
}

namespace p_variation
{
	static void update_root(move mv, move_var& pv, const search::node::p_variation* pv_next, key64& key)
	{
		// updating the whole PV if a new best root move has been found

		pv.pos_key = key;
		pv.cnt = 1 + pv_next->cnt;
		pv.mv[0] = mv;
		int pv_max{ std::min(pv_next->cnt, lim::dt - 1) };
		for (int i{}; i < pv_max; ++i)
			pv.mv[i + 1] = pv_next->mv[i];
	}

	static void update_leaf(move mv, search::node::p_variation* pv, const search::node::p_variation* pv_next)
	{
		// updating the provisional PV during the search

		pv->cnt = 1 + pv_next->cnt;
		pv->mv[0] = mv;
		int pv_max{ std::min(pv_next->cnt, lim::dt - 1) };
		for (int i{}; i < pv_max; ++i)
			pv->mv[i + 1] = pv_next->mv[i];
	}
}

void search::init_params()
{
	// initializing various parameters

	for (int dt{}; dt <= lim::dt; ++dt)
		for (int mv{}; mv < (int)lim::moves; ++mv)
			late_move_red[dt][mv] = depth(LMR_START / 100.0 + LMR_BASE / 100.0 * std::log(dt) * std::log(mv));

	late_move_cnt =
	{ {
		{{ 0, LMP01, LMP02, LMP03, LMP04, LMP05, LMP06 }},
		{{ 0, LMP11, LMP12, LMP13, LMP14, LMP15, LMP16 }}
	} };

	attack::value =
	{ {
		(score)(int)SEE_V_PAWN, (score)(int)SEE_V_KNIGHT, (score)(int)SEE_V_BISHOP,
		(score)(int)SEE_V_ROOK, (score)(int)SEE_V_QUEEN,  (score)10000, (score)0
	} };

	history::w =
	{
		HIST_BASE, HIST_GRAVITY, BASE_BONUS, BASE_MALUS, CORR_BASE, CORR_GRAVITY, CORR_BONUS,
		{ CORR_PAWN,  CORR_MINOR, CORR_MAJOR, CORR_NONPAWN_W, CORR_NONPAWN_B, 
		  CORR_CONT2, CORR_CONT3, CORR_CONT4, CORR_CONT5 }
	};

	timemanage::target_moves  = TARGET_MOVES;
	timemanage::tolerable_div = TOLERABLE_DIV;
	chronometer::extend_time  = EXTEND_TIME;
}

score search::qsearch(sthread& thread, sstack* stack, node nd, depth dt, score alpha, score beta)
{
	// quiescence search at leaf nodes

	verify(-MATE <= alpha && alpha < beta && beta <= MATE);
	verify(dt <= 0);
	thread.cnt_n += 1;
	thread.seldt = std::max(thread.seldt, stack->dt);
	nd.p_var->cnt = 0;

	board& pos{ *nd.pos };
	thread.check_expiration();

	// detecting draws (~10 Elo) & making sure that the bounds don't exceed the score of the shortest possible mate
	
	if (pos.draw(thread.rep_hash, uci::mv_offset + stack->dt))
		return DRAW;
	
	if (sc::mate_distance_pruning(alpha, beta, stack->dt))
		return alpha;

	// looking for a cutoff through the transposition table (~20 Elo)
	
	trans::entry tt{};
	if (dt == 0 && tt.probe(pos.key.pos, stack->dt) && sc::tt_cutoff(tt.bd, tt.sc, alpha, beta))
		return tt.sc;

	// evaluating the position with the static evaluation
	// the evaluation is then corrected with history tables (~75 Elo)

	score stand_pat{ eval::static_eval(pos, thread.hash) };
	stand_pat = thread.hist.correct_sc(pos, stack, stand_pat);
	score best_sc{ stand_pat };

	if (!nd.check && stand_pat > alpha)
	{
		// standing pat cutoff (~20 Elo)

		if (stand_pat >= beta)
			return stand_pat;
		alpha = stand_pat;
	}

	// generating and sorting moves while looping through them

	node::p_variation new_pv{};
	movepick<mode::LEGAL> pick(pos, nd.check);
	for (move mv{ pick.next() }; mv; mv = pick.next())
	{
		verify(dt == 0 || !mv.quiet());

		if (!nd.check && !mv.quiet())
		{
			// depth limit pruning to prevent quiescence-search explosions

			if (dt <= -6 && !pos.recapture(mv))
				continue;

			// delta pruning (~10 Elo)

			if (stand_pat + attack::value[mv.vc()] + DELTA_MARGIN < alpha && !mv.promo())
				continue;

			// SEE pruning (~35 Elo)

			if (!attack::see_above(pos, mv, score(0)))
				continue;
		}

		pos.new_move(mv);
		verify(pos.legal());

		stack->mv = mv;
		stack->cont_mv = &thread.hist.corr_cont[mv.pc()][mv.sq2()];
		node new_nd{ &pos, &new_pv, false, false, false };

		score sc{ -qsearch(thread, stack + 1, new_nd, dt - 1, -beta, -alpha) };
		pos = pick.list.pos;
		verify(type::sc(sc));

		if (sc > best_sc)
		{
			best_sc = sc;
			if (sc > alpha)
			{
				alpha = sc;
				if (sc >= beta)
					return sc;
				p_variation::update_leaf(mv, nd.p_var, &new_pv);
			}
		}
	}

	// checkmate detection is possible only in the first ply after the main search
	// at deeper plies, check detection is skipped
	
	if (nd.check && pick.hits == 0)
		return score(stack->dt) - MATE;
	
	verify(type::sc(best_sc));
	return best_sc;
}

namespace search
{
	static score alphabeta(sthread& thread, sstack* stack, node nd, depth dt, score alpha, score beta)
	{
		// main alpha-beta search

		verify(-MATE <= alpha && alpha < beta&& beta <= MATE);
		verify(!(beta != alpha + 1 && nd.cut));
		verify(dt <= lim::dt);

		// dropping into quiescence search at leaf nodes (~220 Elo)

		if (dt <= 0 || stack->dt >= lim::dt)
			return qsearch(thread, stack, nd, 0, alpha, beta);

		thread.cnt_n += 1;
		nd.p_var->cnt = 0;
		nd.pv = (beta != alpha + 1);
		board& pos{ *nd.pos };
		thread.check_expiration();

		// detecting draws (~45 Elo) & making sure that the bounds don't exceed the shortest possible mate-score

		if (pos.draw(thread.rep_hash, uci::mv_offset + stack->dt))
			return DRAW;

		if (sc::mate_distance_pruning(alpha, beta, stack->dt))
			return alpha;

		// probing the transposition table (single threaded ~190 Elo)
		// checking for a cutoff if probing succeeds (~95 Elo)

		key64 key{ zobrist::adjust_key(pos.key.pos, stack->singular_mv) };
		trans::entry tt{};

		if (tt.probe(key, stack->dt) && !nd.pv && tt.dt >= dt)
			if ((tt.sc <= alpha || tt.sc >= beta) && sc::tt_cutoff(tt.bd, tt.sc, alpha, beta))
				return tt.sc;

		// probing Syzygy endgame table-bases

		if (auto sc{ thread.probe_syzygy(pos, dt, stack->dt) }; std::get<0>(sc) != score::NONE)
		{
			trans::store(key, move{}, sc, lim::dt - 1, stack->dt);
			return std::get<0>(sc);
		}

		// evaluating the current position, correcting the static eval with history tables (~20 Elo)

		node::p_variation new_pv{};
		score sc{ nd.check ? score::NONE : eval::static_eval(pos, thread.hash)};
		stack->sc = sc = thread.hist.correct_sc(pos, stack, sc);
		(stack + 1)->killer = {};
		(stack + 2)->fail_high_cnt = 0;

		bool pruning  { !nd.pv && !nd.check && stack->pruning && !sc::mate(beta) };
		bool improving{ (stack - 2)->sc != score::NONE && (stack - 2)->sc < sc };
		bool critical { nd.pv || nd.check || improving };

		// internal iterative reduction (~10 Elo)

		if (nd.cut && dt >= IIR_DT && !tt.mv)
			dt -= 1;

		// static null move pruning (~35 Elo)

		if (pruning && dt <= SNMP_DT && sc - SNMP_MARGIN1 * dt - SNMP_MARGIN2 * dt * dt >= beta)
			return (sc + beta) / 2;

		// razoring (~15 Elo)

		if (pruning && dt <= RAZOR_DT)
			if (score new_alpha{ alpha - RAZOR_MARGIN1 - RAZOR_MARGIN2 * dt }; sc <= new_alpha)
			{
				score new_score{ qsearch(thread, stack, nd, 0, new_alpha, new_alpha + score(1)) };
				if (new_score <= new_alpha)
					return new_score;
			}

		// null move pruning (~85 Elo)

		if (pruning && dt >= NMP_DT && !stack->singular_mv && !pos.lone_pawns() && sc >= beta)
		{
			depth red{ NMP_RED1 + dt / NMP_RED2 + std::min(3, depth(sc - beta) / NMP_RED3) };
			null::make_move(pos, stack, thread);
			node  new_nd{ &pos, &new_pv, false, !nd.cut, false };
			score null_sc{ -alphabeta(thread, stack + 1, new_nd, dt - red, -beta, score(1) - beta) };
			null::revert_move(pos, stack);

			if (null_sc >= beta)
				return beta;
		}

		// internal iterative deepening (~5 Elo)

		if (stack->pruning && nd.pv && !tt.mv && dt >= IID_DT)
		{
			stack->pruning = false;
			node new_nd{ &pos, &new_pv, nd.check, nd.cut, false };
			alphabeta(thread, stack, new_nd, dt - IID_RED, alpha, beta);
			stack->pruning = true;
			tt.probe(key, stack->dt);
		}

		// initializing move loop

		move& counter{ thread.counter[pos.cl][(stack - 1)->mv.pc()][(stack - 1)->mv.sq2()] };
		move  best_mv{};
		score best_sc{ -MATE };
		score old_alpha{ alpha };
		int quiet_cnt{}, capture_cnt{};

		// generating and sorting moves while looping through them

		movepick<mode::PSEUDOLEGAL> pick(pos, tt.mv, stack, counter, thread.hist);
		for (move mv{ pick.next() }; mv; mv = pick.next())
		{
			verify(pick.hits >= 1 && pick.hits <= int(lim::moves));
			verify(pick.hits == 1 || mv != tt.mv);

			// skipping moves that are being verified to be singular

			if (mv == stack->singular_mv)
			{
				pick.hits -= 1;
				continue;
			}

			// initializing for the new move

			bool gives_check{ pos.gives_check(mv) };
			bool quiet{ mv.quiet() };
			history::sc hist{};
			if (quiet)
				hist.get(thread.hist, mv, stack);
			pruning = !nd.pv && best_sc > -LONGEST_MATE;

			// pruning quiet moves at shallow depth (~95 Elo)

			if (pruning && quiet && !gives_check && !nd.check)
			{
				// late move pruning (~50 Elo)

				if (dt <= LMP_DT && pick.hits >= late_move_cnt[critical][dt])
					continue;

				// futility pruning (~15 Elo)

				if (dt <= FUT_DT && stack->sc + FUT_MARGIN1 + FUT_MARGIN2 * dt <= alpha)
					continue;

				// continuation-history pruning (~2 Elo)

				if (dt <= CONT_HIST_DT && (hist.cont1 < CONT_HIST_MARGIN1 || hist.cont2 < CONT_HIST_MARGIN2))
					continue;

				// SEE pruning quiets (~5 Elo)

				if (dt <= SEE_QUIET_DT && !attack::see_above(pos, mv, score(SEE_QUIET_MARGIN * dt)))
					continue;
			}

			// main history pruning (~0 Elo)

			if (best_sc > -LONGEST_MATE && quiet && dt <= HIST_DT && hist.main < HIST_MARGIN)
				continue;

			// SEE pruning bad tactical moves (~10 Elo)

			if (pruning && !quiet && dt <= SEE_TAC_DT && !attack::see_above(pos, mv, score(SEE_TAC_MARGIN * dt)))
				continue;

			// singular extension (~2 Elo)

			depth ext{};
			if (mv == tt.mv && dt >= SE_DT && tt.bd == bound::LOWER && tt.dt >= dt)
			{
				verify(pick.hits == 1);
				verify(tt.sc != score::NONE);

				score alpha_bd{ std::max(tt.sc - score(dt), -MATE) };
				stack->singular_mv = mv;
				node new_nd{ &pos, &new_pv, nd.check, nd.cut, false };
				sc = -alphabeta(thread, stack, new_nd, dt - SE_RED, alpha_bd, alpha_bd + score(1));

				verify(stack->singular_mv == mv);
				stack->singular_mv = move{};
				if (sc <= alpha_bd)
					ext = 1;
				else if (tt.sc <= alpha)
					ext = -1;
			}

			// other extensions: check (~40 Elo) & history (~10 Elo)

			if (gives_check || (mv == tt.mv && dt >= HIST_EXT_DT && hist.all() >= HIST_EXT_MARGIN && std::abs(tt.sc) <= 500))
				ext = 1;

			// speculative prefetch of the next TT-entry (~15 Elo)

			key64 next_key{ zobrist::pos_key(pos, mv) };
			memory::prefetch((char*)trans::get_entry(next_key));

			// doing the move and checking if it is legal

			pos.new_move(mv);
			if (!pos.legal())
			{
				pick.revert(pos);
				continue;
			}
			stack->mv = mv;
			stack->cont_mv = &thread.hist.corr_cont[mv.pc()][mv.sq2()];
			if (quiet)
				stack->quiet_mv[quiet_cnt++] = mv;
			else if (mv.capture())
				stack->capture_mv[capture_cnt++] = mv;
			node new_nd{ &pos, &new_pv, gives_check, true, false };
			verify(gives_check == pos.check());

			// late move reduction (~180 Elo)

			bool reducing{};
			depth new_dt{ dt - 1 + ext };
			depth red_dt{};

			if ((reducing = pick.hits >= LMR_CNT && dt >= LMR_DT))
			{
				if (quiet)
				{
					red_dt  = late_move_red[dt][pick.hits];
					red_dt += nd.cut;
					red_dt += !nd.pv;
					red_dt += hist.all() / HIST_RED;
					red_dt += alpha - stack->sc > ALPHA_MARGIN;
					red_dt -= !nd.cut && attack::escape(pick.list.pos, mv);
					red_dt -= (stack + 1)->fail_high_cnt <= FAIL_HIGH_CNT;
				}
				else
				{
					red_dt = nd.cut
						|| alpha - stack->sc >= attack::value[mv.vc()]
						|| pick.hits >= late_move_cnt[critical][std::min(dt, 6)];
				}

				red_dt += !critical;
				red_dt  = std::max(new_dt - std::clamp(red_dt, -1, int(LMR_MAX)), 1);

				// searching with reduced depth on a null window for late moves & adjusting depth afterwards (~10 Elo)

				sc = -alphabeta(thread, stack + 1, new_nd, red_dt, -alpha - score(1), -alpha);
				new_dt += (sc > best_sc + DEEPER_MARGIN + dt * 2);
				new_dt -= (sc < best_sc + SHALLOWER_MARGIN + dt);
			}

			// LMR-research & principal variation search (~205 Elo)

			if (!(reducing && sc <= alpha))
			{
				new_nd.cut =   { nd.pv && pick.hits == 1 ? false : !nd.cut };
				score new_alpha{ nd.pv && pick.hits  > 1 ? -alpha - score(1) : -beta };

				if (!reducing || new_dt > red_dt)
					sc = -alphabeta(thread, stack + 1, new_nd, new_dt, new_alpha, -alpha);

				if (pick.hits > 1 && nd.pv && sc > alpha)
				{
					new_nd.cut = false;
					sc = -alphabeta(thread, stack + 1, new_nd, new_dt, -beta, -alpha);
				}
			}

			pos = pick.list.pos;
			verify(type::sc(sc));

			// checking for a new best move

			if (sc > best_sc)
			{
				best_sc = sc;
				if (sc > alpha)
				{
					// update history tables

					best_mv = mv;
					thread.hist.update(mv, stack, quiet_cnt, capture_cnt, dt);

					// checking for a beta cutoff
					
					if (sc >= beta)
					{
						if (quiet)
						{
							stack->killer.update(mv);
							counter = mv;
						}
						stack->fail_high_cnt += 1;
						break;
					}
					alpha = sc;
					p_variation::update_leaf(mv, nd.p_var, &new_pv);
				}
			}
		}

		// detecting checkmate & stalemate

		if (pick.hits == 0)
		{
            verify(alpha == old_alpha);
            return !stack->singular_mv ? (nd.check ? score(stack->dt) - MATE : DRAW) : alpha;
		}

		// storing the results in the transposition table

		if (!stack->singular_mv)
			trans::store(key, best_mv, sc::make_bounded(best_sc, old_alpha, beta), dt, stack->dt);

		// updating correction history tables

		if (!nd.check && !(best_mv && best_mv.capture())
			&& ((best_sc < stack->sc && best_sc < beta) || (best_sc > stack->sc && best_mv)))
			thread.hist.update_corr(pos, stack, best_sc, stack->sc, dt);

		verify(type::sc(best_sc));
		return best_sc;
	}

	static score alphabeta_root(sthread& thread, board& pos, rootpick& pick, depth dt, score alpha, score beta, int multipv)
	{
		// starting the alpha-beta search with the root nodes

		verify(1 <= dt && dt <= lim::dt);
		verify(-MATE <= alpha && alpha < beta && beta <= MATE);

		// initializing the search

		auto stack{ thread.stack_front() };
		(stack + 1)->killer = {};
		(stack + 2)->fail_high_cnt = 0;
		node::p_variation pv{};
		score sc{ score::NONE };
		int mv_n{};

		// looping through the move-list

		for (auto root{ pick.first() }; root; root = pick.next())
		{
			if (root->skip)
				continue;
			mv_n += 1;

			uci::info_currmove(thread, multipv, root->mv, mv_n);
			root->nodes -= thread.cnt_n;

			pos.new_move(root->mv);
			verify(pos.legal());
			node new_nd{ &pos, &pv, root->check, mv_n > 1, false };
			stack->mv = root->mv;
			stack->cont_mv = &thread.hist.corr_cont[root->mv.pc()][root->mv.sq2()];

			// check extension (~0 Elo)

			depth ext{ root->check };
			score new_alpha{ mv_n > 1 ? -alpha - score(1) : -beta };
			
			// PVS (~95 Elo)

			sc = -alphabeta(thread, stack + 1, new_nd, dt - 1 + ext, new_alpha, -alpha);
			if (mv_n > 1 && sc > alpha)
			{
				new_nd.cut = false;
				sc = -alphabeta(thread, stack + 1, new_nd, dt - 1 + ext, -beta, -alpha);
			}

			root->nodes += thread.cnt_n;
			pick.revert(pos);

			// refining the search scores with the more accurate table-base scores

			bool refinable{ pick.tb_pos && sc::tb::refinable(root->weight, sc) };
			if  (refinable)
				sc = (score)root->weight;

			verify(root->nodes >= 0);
			verify(type::sc(sc));

			if (sc > alpha)
			{
				if (sc >= beta)
					return sc;

				// new best move was found, so updating the PV
				// adding a bonus to favor the move in the next rearrangement of the move order (~0 Elo)

				alpha = sc;
				p_variation::update_root(root->mv, thread.pv[multipv], &pv, pos.key.pos);
				if (pick.tb_pos)
					thread.pv[multipv].tb_root = refinable;

				root->nodes += thread.cnt_n;
			}
		}
		return alpha;
	}

	static score aspiration_window(sthread& thread, board& pos, rootpick& pick, depth base_dt, int multipv)
	{
		// entering the alpha-beta-search through an aspiration window (~25 Elo)
		// the window widens dynamically depending on the bound the search returns

		auto& pv{ thread.pv[multipv] };
		bound bd{ bound::EXACT };
		depth dt{ base_dt };

		score alpha { -MATE }, beta { MATE };
		score sc    { score::NONE }, sc_old{ pv.sc };
		score window{ score(int(search::ASP_WINDOW)) }, margin{ window };

		verify(dt == 1 || pv.sc != score::NONE);

		// table-base positions always get an infinite window to encourage finding the correct move by the search itself
		// proven mates also get an infinite window

		if (dt >= 4 && !pick.tb_pos && !sc::tb::mate(sc_old) && !sc::mate(sc_old))
		{
			alpha = std::max(sc_old - margin, -MATE);
			beta  = std::min(sc_old + margin,  MATE);
		}

		while (true)
		{
			verify(-MATE <= alpha && alpha < beta && beta <= MATE);
			sc = alphabeta_root(thread, pos, pick, std::max(1, dt), alpha, beta, multipv);

			verify(type::sc(sc));
			margin = margin * search::ASP_MULT;

			if (sc <= alpha)
			{
				bd    = bound::UPPER;
				beta  = (beta + alpha) / 2;
				alpha = std::max(sc - margin, -MATE);
				dt    = base_dt;
			}
			else if (sc >= beta)
			{
				bd   = bound::LOWER;
				beta = std::min(sc + margin, MATE);
				dt  -= 1;
			}
			else
				break;

			if (margin > window * search::ASP_MULT_MAX)
			{
				alpha = -MATE;
				 beta =  MATE;
			}
			uci::info_bound(thread, multipv, sc, bd);
		}
		return sc;
	}

	static void iterative_deepening(sthread& thread)
	{
		// iterative deepening framework, the base of the search hierarchy
		// starting by generating all root node moves

		board pos{ thread.pos };
		rootpick pick(pos);
		thread.cnt_root_mv = pick.mv_cnt();

		// probing Syzygy table-bases
		
		thread.use_syzygy = syzygy::probe_root(pos, pick);
		thread.cnt_tbhit += pick.tb_pos;

		// starting iterative deepening & looping through all principal variations indicated by UCI command 'MultiPV'

		score sc{ score::NONE };
		for (depth dt{ 1 }; dt <= uci::limit.dt && !uci::stop; ++dt)
		{
			for (int i{}; i < (int)uci::multipv && i < pick.mv_cnt() && !uci::stop; ++i)
			{
				// rearranging the root move order (~165 Elo) before starting the alpha-beta search through an aspiration window

				move_var& pv{ thread.pv[i] };
				pv.dt = dt;
				pick.rearrange_list(pv.mv[0], i > 0 ? thread.pv[i - 1].mv[0] : move{});

				try { sc = aspiration_window(thread, pos, pick, dt, i); }
				catch ([[maybe_unused]] exception& ex)
				{
					verify(ex == exception::STOP_SEARCH);
					pick.revert(pos);
				}

				// extending the targeted search time if the score is dropping (~5 Elo)

				if (thread.main() && dt >= EXT_TIME_DT && sc - pv.sc <= EXT_TIME_MARGIN)
					thread.extend_time(sc - pv.sc);

				if (sc != -MATE)
					pv.sc = sc;
			}

			// providing search information at every iteration & checking the search expiration conditions

			if (thread.main())
				uci::info_iteration(thread);
			if (abort(thread.chrono, thread.pv[0].mv, pick, dt, thread.pv[0].sc))
				break;
		}
	}
}

void sthread::start_search()
{
	// each thread is starting the search

	thread_pool::searching += 1;
	init();
	search::iterative_deepening(*this);

	// search has finished
	// forcing early search termination for all other threads if some are already idle

	thread_pool::searching -= 1;
	if (!uci::stop && thread_pool::searching < (int)pool->size() * 3 / 4)
		uci::stop = true;

	// waiting until all threads have finished their search

	std::unique_lock<std::mutex> lock(thread_pool::mutex);
	if (main())
		while (thread_pool::searching)
			thread_pool::cv.wait(lock);
	else
		thread_pool::cv.notify_all();
}

void search::start(thread_pool& threads, timemanage::move_time mv_time)
{
	// starting point of the search hierarchy
	// all search threads are activated and the search begins

	verify(uci::limit.dt >= 1);
	verify(uci::limit.dt <= lim::dt);

	threads.start_clock(mv_time);
	for (uint32 i{ 0 + 1 }; i < threads.thread.size(); ++i)
		threads.thread[i]->awake();
	threads.thread[0]->start_search();

	// concluding the search

	bench += threads.thread[0]->get_nodes();

	// taking care of an early search termination after the UCI 'go ponder' or 'go infinite' command
	// returning a best-move only after the 'ponderhit' or 'stop' command

	std::unique_lock<std::mutex> lock(uci::mutex);
	while (uci::infinite && !uci::stop)
		uci::cv.wait(lock);

	uci::stop = true;
	uci::infinite = false;
	uci::info_bestmove(threads.get_bestmove());
}