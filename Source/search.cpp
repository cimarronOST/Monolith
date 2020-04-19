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


#include "zobrist.h"
#include "misc.h"
#include "bit.h"
#include "syzygy.h"
#include "movepick.h"
#include "movegen.h"
#include "move.h"
#include "attack.h"
#include "trans.h"
#include "eval.h"
#include "uci.h"
#include "search.h"

int64 search::bench{};

namespace sc
{
	bool mate(score sc)
	{
		// checking whether the score is a mate score

		verify(type::sc(sc));
		return std::abs(sc) >= score::longest_mate;
	}

	bool draw(score sc)
	{
		// checking whether the score is a draw score

		verify(type::sc(sc));
		return sc == score::draw;
	}

	bool good_enough_mate(score sc)
	{
		// checking whether the score is a good enough mate as defined by the UCI 'go mate' command

		verify(type::sc(sc));
		return score::mate - sc <= uci::limit.mate * 2;
	}

	score alpha_bound(score alpha, depth curr_dt)
	{
		// setting a bound for alpha

		return std::max(-score::mate + score(curr_dt), alpha);
	}

	score beta_bound(score beta, depth curr_dt)
	{
		// setting a bound for beta

		return std::min(score::mate - score(curr_dt + 1), beta);
	}
}

namespace sc::tb
{
	bool mate(score sc)
	{
		// checking whether the score is a table-base win or loss

		verify(type::sc(sc));
		return std::abs(sc) >= score::tb_win - lim::dtz && std::abs(sc) <= score::tb_win + lim::dtz;
	}

	bool draw(score sc)
	{
		// checking whether the score is a table-base draw score

		verify(type::sc(sc));
		return !mate(sc);
	}

	std::tuple<score, bound> wdl(int wdl, depth curr_dt)
	{
		// converting the table-base WDL score into a more convenient score and bound

		verify (wdl >= -2 && wdl <= 2);
		switch (wdl)
		{
		case  2: return { score::tb_win  - score(curr_dt), bound::lower };
		case -2: return { score::tb_loss + score(curr_dt), bound::upper };
		default: return { score::draw    + score(wdl),     bound::exact };
		}
	}
}

namespace null
{
	void make_move(board& pos, sthread::sstack* stack)
	{
		// doing a "null move"

		stack->null_mv = sthread::sstack::null_move{};
		pos.null_move(stack->null_mv.ep, stack->null_mv.sq);
		(stack + 1)->pruning = false;
	}

	void revert_move(board& pos, sthread::sstack* stack)
	{
		// reverting the "null move"

		pos.revert_null_move(stack->null_mv.ep, stack->null_mv.sq);
		(stack + 1)->pruning = true;
	}
}

namespace expiration
{
	// monitoring search expiration

	bool abort(const chronometer& chrono, const std::array<move, lim::dt>& pv_mv, const rootpick& pick, depth dt, score sc)
	{
		// checking the criteria for early search abortion

		verify(type::dt(dt));
		verify(type::sc(sc));
		milliseconds time{ chrono.elapsed() };

		// special cases are UCI commands 'go infinite', 'go ponder', 'go mate' & 'go movetime'

		if (uci::infinite)
			return false;
		if (sc::good_enough_mate(sc))
			return true;
		if (chrono.movetime.fixed())
			return time >= chrono.movetime.target;

		// otherwise making sure that easy moves are searched to a lesser extend

		return  time > chrono.movetime.target /  2
			|| (time > chrono.movetime.target /  8 && sc > score::longest_mate)
			|| (time > chrono.movetime.target /  8 && pick.tb_pos)
			|| (time > chrono.movetime.target / 16 && dt > 8 && !pv_mv[dt - 8])
			|| (time > chrono.movetime.target / 32 && pick.single_reply());
	}

	bool stop_thread(sthread& thread)
	{
		// keeping track of the elapsing time
		// also increasing the frequency of checking the elapsed time while probing Syzygy tablebases

		if (++thread.chrono.hits < thread.chrono.hit_threshold)
			return false;
		thread.chrono.hits = 0;
		if (uci::infinite)
			return false;
		if (thread.get_nodes() >= uci::limit.nodes)
			return true;

		return thread.chrono.elapsed() >= thread.chrono.movetime.target;
	}

	void check(sthread& thread)
	{
		// checking for immediate search termination

		if (uci::stop || stop_thread(thread))
		{
			uci::stop = true;
			throw exception::stop_search;
		}
	}
}

namespace update
{
	void killer_mv(killer_list& killer, move mv)
	{
		// updating killer-moves if a quiet move fails high

		if (mv != killer[0])
		{
			killer[1] = killer[0];
			killer[0] = mv;
		}
	}

	void main_pv(move mv, move_var& pv, const search::p_variation& pv_next)
	{
		// updating the whole PV if a new best root move has been found

		pv.cnt = 1 + pv_next.cnt;
		pv.mv[0] = mv;
		for (int i{}; i < pv_next.cnt; ++i)
			pv.mv[i + 1] = pv_next.mv[i];
	}

	void prov_pv(move mv, search::p_variation& pv, const search::p_variation& pv_next)
	{
		// updating the provisional PV during the search

		pv.cnt = 1 + pv_next.cnt;
		pv.mv[0] = mv;
		for (int i{}; i < pv_next.cnt; ++i)
			pv.mv[i + 1] = pv_next.mv[i];
	}
}

namespace late_move
{
	// late move pruning move count, indexed by depth

	constexpr std::array<std::array<depth, 7>, 2> cnt
	{ {
		{{ 0, 3, 4,  7, 11, 17, 24 }},
	    {{ 0, 6, 8, 12, 18, 30, 44 }}
	} };

	// late move reduction table indexed by move-count and depth

	std::array<std::array<depth, lim::moves>, lim::dt + 1> red{};
}

void search::init_tables()
{
	// initialising the LMR-table at startup

	for (int dt{}; dt <= lim::dt; ++dt)
		for (int mv{}; mv < (int)lim::moves; ++mv)
			late_move::red[dt][mv] = depth(0.75 + 0.5 * log(dt) * log(mv));
}

namespace abdada
{
	// simplified ABDADA
	// idea for the implementation of this parallelization-algorithm from Tom Kerrigan:
	// http://www.tckerrigan.com/Chess/Parallel_Search/Simplified_ABDADA

	constexpr depth dt_defer { 3 };
	constexpr depth dt_cutoff{ 4 };
	constexpr uint32 size{ 1U << 15 };
	constexpr uint32 mask{ size - 1 };

	std::array<std::array<key32, 4>, size> concurrent{};

	bool defer(key32 mv)
	{
		// checking whether a move is already being searched by another thread
		// if true, the move gets deferred

		auto& entry{ concurrent[mv & mask] };
		return std::any_of(entry.begin(), entry.end(), [&](key32 entry_mv) { return entry_mv == mv; });
	}

	void add(key32 mv)
	{
		// adding a move to the list of currently searched moves

		auto &slots{ concurrent[mv & mask] };
		for (auto &entry : slots)
		{
			if (entry == 0U)
			{
				entry = mv;
				return;
			}
			if (entry == mv)
				return;
		}
		slots[0] = mv;
	}

	void remove(key32 mv)
	{
		// removing a move from the list of currently searched moves

		auto& slots{ concurrent[mv & mask] };
		for (auto &entry : slots)
		{
			if (entry == mv)
				entry = 0U;
		}
	}
}

score search::qsearch(sthread& thread, board& pos, p_variation& pv, bool check, depth stack_dt, depth dt, score alpha, score beta)
{
	// quiescence search at the leaf nodes

	verify(-score::mate <= alpha && alpha < beta&& beta <= score::mate);
	verify(dt <= 0);
	thread.cnt_n += 1;
	pv.cnt = 0;
	expiration::check(thread);

	// detecting draws

	if (pos.draw(thread.rep_hash, uci::mv_offset + stack_dt))
		return score::draw;

	// mate distance pruning

	alpha = sc::alpha_bound(alpha, stack_dt);
	beta  = sc::beta_bound( beta,  stack_dt);
	if (alpha >= beta)
		return alpha;

	// transposition table lookup (~27 Elo)

	trans::entry tt{};
	if (dt == 0 && tt.probe(pos.key, stack_dt))
	{
		if (tt.bd == bound::exact
		|| (tt.bd == bound::lower && tt.sc >= beta)
		|| (tt.bd == bound::upper && tt.sc <= alpha))
			return tt.sc;
	}

	// evaluating the position

	score stand_pat{ eval::static_eval(pos, thread.hash) };
	score best_sc  { stand_pat };

	if (!check && stand_pat > alpha)
	{
		// standing pat cutoff (~108 Elo)

		if (stand_pat >= beta)
			return stand_pat;
		alpha = stand_pat;
	}

	// generating and sorting moves while looping through them

	p_variation pv_next{};
	movepick<mode::legal> pick(pos, check);
	for (move mv{ pick.next() }; mv; mv = pick.next())
	{
		verify(dt == 0 || !mv.quiet());
		verify_deep(pos.pseudolegal(mv));

		if (!check && !mv.quiet())
		{
			// depth limit pruning (~13 Elo)

			if (dt <= -6 && !pos.recapture(mv))
				continue;

			// delta pruning (~23 Elo)

			if (stand_pat + attack::value[mv.vc()] + 100 < alpha && !mv.promo())
				continue;

			// SEE pruning (~69 Elo)

			if (!attack::see_above(pos, mv, score(0)))
				continue;
		}

		pos.new_move(mv);
		verify_deep(pos.legal());

		score sc{ -qsearch(thread, pos, pv_next, false, stack_dt + 1, dt - 1, -beta, -alpha) };
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
				update::prov_pv(mv, pv, pv_next);
			}
		}
	}

	// checkmate detection is possible only in the first ply after the main search
	// at deeper plies, check detection is skipped

	if (check && pick.hits == 0)
		return score(stack_dt) - score::mate;

	verify(type::sc(best_sc));
	return best_sc;
}

namespace search
{
	score alphabeta(sthread& thread, board& pos, sthread::sstack* stack, p_variation& pv, bool in_check, bool cut_node, depth dt, score alpha, score beta)
	{
		// main alpha-beta search
		// first dropping into quiescence search at leaf nodes

		verify(-score::mate <= alpha && alpha < beta&& beta <= score::mate);
		verify(!(beta != alpha + 1 && cut_node));
		verify(dt <= lim::dt);

		if (dt <= 0 || stack->dt >= lim::dt)
			return qsearch(thread, pos, pv, in_check, stack->dt, 0, alpha, beta);

		thread.cnt_n += 1;
		pv.cnt = 0;
		expiration::check(thread);

		// detecting draws

		if (pos.draw(thread.rep_hash, uci::mv_offset + stack->dt))
			return score::draw;

		// mate distance pruning

		alpha = sc::alpha_bound(alpha, stack->dt);
		beta  = sc::beta_bound( beta,  stack->dt);
		if (alpha >= beta)
			return alpha;

		// probing transposition table (~188 Elo)

		bool pv_node{ beta != alpha + 1 };
		key64 key{ zobrist::adjust_key(pos.key, stack->singular_mv) };
		trans::entry tt{};
		if (tt.probe(key, stack->dt) && !pv_node && tt.dt >= dt && (tt.sc <= alpha || tt.sc >= beta))
		{
			// cutoff (~98 Elo)

			if (tt.bd == bound::exact
			|| (tt.bd == bound::lower && tt.sc >= beta)
			|| (tt.bd == bound::upper && tt.sc <= alpha))
				return tt.sc;
		}

		// probing Syzygy tablebases

		if (thread.use_syzygy)
		{
			if (int pop{ bit::popcnt(pos.side[both]) };
				pop <= uci::syzygy.pieces
				&& (dt >= uci::syzygy.dt || pop < std::min(5, uci::syzygy.pieces))
				&& pos.half_cnt == 0)
			{
				thread.chrono.hits = thread.chrono.hit_threshold;
				int success{};
				if (int wdl{ syzygy::probe_wdl(pos, success) }; success)
				{
					thread.cnt_tbhit += 1;
					auto  tb{ sc::tb::wdl(wdl, stack->dt) };
					score sc{ std::get<0>(tb) };
					bound bd{ std::get<1>(tb) };

					trans::store(key, move{}, sc, bd, lim::dt - 1, stack->dt);
					return sc;
				}
			}
		}

		// evaluating the current position & initializing pruning

		p_variation pv_next{};
		score sc{ (pv_node || in_check) ? score::none : eval::static_eval(pos, thread.hash) };
		stack->sc = sc;

		bool pruning{ !pv_node && !in_check && stack->pruning && !sc::mate(beta) };
		bool critical{ pv_node ||  in_check || stack->dt <= 2 || (stack - 2)->sc == score::none || (stack - 2)->sc < sc };

		// static null move pruning (~58 Elo)

		if (pruning && dt <= 3 && sc - 50 * dt >= beta)
			return beta;

		// razoring (~2 Elo)

		if (pruning && dt <= 2 && sc + 200 + 100 * dt <= alpha)
		{
			score raz_alpha{ score(alpha - 200 - 100 * dt) };
			score new_score{ qsearch(thread, pos, pv, in_check, stack->dt, 0, raz_alpha, raz_alpha + score(1)) };
			if (new_score <= raz_alpha)
				return new_score;
		}

		// null move pruning (~53 Elo)

		if (pruning && dt >= 2 && !stack->singular_mv && !pos.lone_pawns() && sc >= beta)
		{
			depth red{ 2 + dt / 5 + std::min(3, depth(sc - beta) / 150) };
			null::make_move(pos, stack);
			stack->mv = move{};
			score null_sc{ -alphabeta(thread, pos, stack + 1, pv_next, false, !cut_node, dt - 1 - red, -beta, score(1) - beta) };
			null::revert_move(pos, stack);

			if (null_sc >= beta)
				return beta;
		}

		// internal iterative deepening (~3 Elo)

		if (stack->pruning && pv_node && !tt.mv && dt >= 3)
		{
			stack->pruning = false;
			alphabeta(thread, pos, stack, pv_next, in_check, cut_node, dt - 2, alpha, beta);
			stack->pruning = true;
			tt.probe(key, stack->dt);
		}

		// initializing move loop
		// to index history tables, the two previous moves are retrieved

		move best_mv{};
		move mv_prev1{ (stack - 1)->mv };
		move mv_prev2{ stack->dt >= 2 ? (stack - 2)->mv : move{} };
		move& counter{ thread.counter[pos.cl][mv_prev1.pc()][mv_prev1.sq2()] };

		score best_sc{ -score::mate };
		score futility_sc{ score(sc + 50 + 100 * dt) };
		score old_alpha{ alpha };

		int defer_cnt{};
		int quiet_cnt{};
		int second_pass_cnt{};

		// generating and sorting moves while looping through them

		movepick<mode::pseudolegal> pick(pos, tt.mv, mv_prev1, mv_prev2, counter, stack->killer, thread.hist, stack->defer_mv, defer_cnt);
		for (move mv{ pick.next() }; mv; mv = pick.next())
		{
			verify(pick.hits >= 1 && pick.hits <= int(lim::moves));
			verify(pick.hits == 1 || mv != tt.mv || uci::thread_cnt > 1);
			verify_deep(pos.pseudolegal(mv));

			// cutoff check (ABDADA)

			if (uci::use_abdada && defer_cnt > 0 && !pv_node && dt >= abdada::dt_cutoff)
			{
				verify(uci::thread_cnt > 1);
				if (tt.probe(key, stack->dt) && (tt.bd == bound::lower && tt.sc >= beta))
					return tt.sc;
			}

			// skipping moves that are being proved to be singular

			if (mv == stack->singular_mv)
			{
				pick.hits -= 1;
				continue;
			}

			// initializing for the new move

			int mv_cnt{ pick.stage_deferred() ? stack->mv_cnt[second_pass_cnt++] : pick.hits };
			bool gives_check{ pos.gives_check(mv) };
			bool quiet{ mv.quiet() };
			history::sc hist;
			verify(second_pass_cnt <= defer_cnt);

			if (quiet)
			{
				// retrieving the history scores for quiet moves

				stack->quiet_mv[quiet_cnt++] = mv;
				hist.get(thread.hist, mv, mv_prev1, mv_prev2);
			}

			// pruning quiet moves at shallow depth (~99 Elo)

			if (!pv_node && quiet && best_sc > -score::longest_mate && !gives_check && !in_check)
			{
				// late move pruning (~58 Elo)

				if (dt <= 6 && mv_cnt >= late_move::cnt[critical][dt])
					continue;

				// futility pruning (~4 Elo)

				if (dt <= 6 && futility_sc <= alpha)
					continue;

				// history pruning (~0 Elo)

				if (dt <= 2 && (hist.counter < -500 || hist.continuation < -3000))
					continue;

				// SEE pruning quiets (~15 Elo)

				if (dt <= 10 && !attack::see_above(pos, mv, score(0)))
					continue;
			}

			// SEE pruning bad tactical moves (~10 Elo)

			if (!pv_node && !quiet && best_sc > -score::longest_mate && dt <= 3 && !attack::see_above(pos, mv, score(-100 * dt)))
				continue;

			// deferring moves that are searched by other threads (ABDADA)

			key32 mv_hash{ zobrist::mv_key(mv, pos.key) };
			if (uci::use_abdada && pick.hits > 1 && pick.can_defer() && dt >= abdada::dt_defer && abdada::defer(mv_hash))
			{
				stack->mv_cnt[defer_cnt] = pick.hits;
				stack->defer_mv[defer_cnt++] = mv;
				continue;
			}

			// singular extension (~18 Elo)

			bool singular{};
			if (mv == tt.mv && dt >= 6 && tt.bd == bound::lower && tt.dt >= dt && !defer_cnt)
			{
				verify(pick.hits == 1);
				verify(tt.sc != score::none);

				score alpha_bd{ std::max(tt.sc - score(2 * dt), -score::mate) };
				stack->singular_mv = mv;
				sc = -alphabeta(thread, pos, stack, pv_next, in_check, cut_node, dt - 4, alpha_bd, alpha_bd + score(1));

				verify(stack->singular_mv == mv);
				stack->singular_mv = move{};
				if (quiet)
				{
					verify(quiet_cnt == 1);
					stack->quiet_mv[0] = mv;
				}
				singular = (sc <= alpha_bd);
			}

			// other extensions: check (~39 Elo), pawn-push & recapture (~29 Elo)

			depth ext{ singular
				|| gives_check
				|| (pv_node && (pick.list.pos.recapture(mv) || mv.push_to_7th())) };

			// speculative prefetch of the next TT-entry (~1 Elo)

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
			verify_deep(gives_check == pos.check());

			// late move reduction (~64 Elo)

			depth red{};
			if (mv_cnt >= 4 && dt >= 3)
			{
				if (quiet)
				{
					red  = late_move::red[dt][mv_cnt];
					red += cut_node;
					red += !pv_node;
					red += !critical;
					red += (hist.main + hist.counter + hist.continuation) / -7500;
					red -= !cut_node && attack::escape(pick.list.pos, mv);
					red  = std::min(std::max(red, 0), 6);
				}
				else if (cut_node
					  || stack->sc + attack::value[mv.vc()] <= alpha
					  || mv_cnt >= late_move::cnt[critical][std::min(dt, 6)])
					red = 1;
			}

			// late move reduction search

			depth new_dt{ dt - 1 + ext };
			score new_alpha{ pv_node && pick.hits > 1 ? -alpha - score(1) : -beta };
			bool  new_cut_node{ pv_node && pick.hits == 1 ? false : !cut_node };

			if (red)
				sc = -alphabeta(thread, pos, stack + 1, pv_next, gives_check, true, new_dt - red, -alpha - score(1), -alpha);

			// principal variation search with ABDADA and LMR-research (~427 Elo)

			if (!red || (red && sc > alpha))
			{
				if (uci::use_abdada && pick.hits > 1 && pick.can_defer() && dt > abdada::dt_defer)
				{
					abdada::add(mv_hash);
					sc = -alphabeta(thread, pos, stack + 1, pv_next, gives_check, new_cut_node, new_dt, new_alpha, -alpha);
					abdada::remove(mv_hash);
				}
				else sc = -alphabeta(thread, pos, stack + 1, pv_next, gives_check, new_cut_node, new_dt, new_alpha, -alpha);

				if (pick.hits > 1 && pv_node && sc > alpha)
					sc = -alphabeta(thread, pos, stack + 1, pv_next, gives_check, false, new_dt, -beta, -alpha);
			}

			pos = pick.list.pos;
			verify(type::sc(sc));

			// checking for a new best move

			if (sc > best_sc)
			{
				best_sc = sc;
				if (sc > alpha)
				{
					// checking for a beta cutoff

					best_mv = mv;
					if (sc >= beta)
					{
						if (quiet)
						{
							// updating history tables, killer- & counter-moves (~412 Elo)

							thread.hist.update(mv, mv_prev1, mv_prev2, stack->quiet_mv, quiet_cnt, dt);
							update::killer_mv(stack->killer, mv);
							counter = mv;
						}
						break;
					}
					alpha = sc;
					update::prov_pv(mv, pv, pv_next);
				}
			}
		}

		// detecting checkmate & stalemate

		if (pick.hits == 0)
		{
			verify(alpha == old_alpha);
			return !stack->singular_mv ? (in_check ? score(stack->dt) - score::mate : score::draw) : alpha;
		}

		// storing the results in the transposition table

		if (!stack->singular_mv)
		{
			bound bd{ best_sc <= old_alpha ? bound::upper : (best_sc >= beta ? bound::lower : bound::exact) };
			trans::store(key, best_mv, best_sc, bd, dt, stack->dt);
		}

		verify(type::sc(best_sc));
		return best_sc;
	}

	score alphabeta_root(sthread& thread, board& pos, rootpick& pick, depth dt, score alpha, score beta, int multipv)
	{
		// starting the alpha-beta search with the root nodes

		verify(1 <= dt && dt <= lim::dt);
		verify(-score::mate <= alpha && alpha < beta&& beta <= score::mate);

		// initializing the search

		auto stack{ &(thread.stack[0]) };
		p_variation pv{};
		score sc{ score::none };
		bool wrong_pv{};
		int  mv_n{};

		// looping through the move-list

		for (auto node{ pick.first() }; node; node = pick.next())
		{
			verify_deep(pos.pseudolegal(node->mv));
			if (node->skip)
				continue;
			mv_n += 1;

			uci::info_currmove(thread, multipv, node->mv, mv_n);
			node->nodes -= thread.cnt_n;

			pos.new_move(node->mv);
			stack->mv = node->mv;
			verify_deep(pos.legal());

			// check extension (~7 Elo)

			depth ext{ node->check };
			depth new_dt{ dt - 1 + ext };
			score new_alpha{ mv_n > 1 ? -alpha - score(1) : -beta };

			// PVS (~117 Elo)

			sc = -alphabeta(thread, pos, stack + 1, pv, node->check, mv_n > 1, new_dt, new_alpha, -alpha);
			if (mv_n > 1 && sc > alpha)
				sc = -alphabeta(thread, pos, stack + 1, pv, node->check, false, new_dt, -beta, -alpha);

			node->nodes += thread.cnt_n;
			pick.revert(pos);

			verify(node->nodes >= 0);
			verify(type::sc(sc));

			if (pick.tb_pos)
			{
				// refining the search score with the more accurate table-base score
				// also noting if the PV doesn't match with this score

				score dtz_sc{ (score)node->weight };
				wrong_pv = (sc::tb::mate(dtz_sc) && !sc::mate(sc)) || (sc::tb::draw(dtz_sc) && !sc::draw(sc));
				if (wrong_pv)
					sc = dtz_sc;
			}

			if (sc > alpha)
			{
				if (sc >= beta)
					return sc;

				alpha = sc;
				update::main_pv(node->mv, thread.pv[multipv], pv);
				node->nodes += thread.cnt_n;

				if (pick.tb_pos)
					thread.pv[multipv].wrong = wrong_pv;
			}
		}
		return alpha;
	}

	score aspiration_window(sthread& thread, board& pos, rootpick& pick, depth base_dt, int multipv)
	{
		// entering the alpha-beta-search through an aspiration window (~20 Elo)
		// the window widens dynamically depending on the bound the search returns

		auto& pv{ thread.pv[multipv] };
		bound bd{ bound::exact };
		depth dt{ base_dt };
		score alpha{ -score::mate }, beta{ score::mate };
		score sc{ score::none }, sc_old{ pv.sc };
		score margin{ score(35) };

		verify(dt == 1 || pv.sc != score::none);

		// table-base positions always get an infinite window to encourage finding the correct move by the search itself
		// proven mates also get an infinite window

		if (dt >= 4 && !pick.tb_pos && !sc::tb::mate(sc_old) && !sc::mate(sc_old))
		{
			alpha = std::max(sc_old - margin, -score::mate);
			beta  = std::min(sc_old + margin,  score::mate);
		}

		while (true)
		{
			verify(-score::mate <= alpha && alpha < beta && beta <= score::mate);
			sc = alphabeta_root(thread, pos, pick, std::max(1, dt), alpha, beta, multipv);

			verify(type::sc(sc));
			margin = margin * 4;

			if (sc <= alpha)
			{
				bd    = bound::upper;
				beta  = (beta + alpha) / 2;
				alpha = std::max(sc - margin, -score::mate);
				dt    = base_dt;
			}
			else if (sc >= beta)
			{
				bd   = bound::lower;
				beta = std::min(sc + margin, score::mate);
				dt  -= 1;
			}
			else
				break;

			if (margin > 140)
			{
				alpha = -score::mate;
				 beta =  score::mate;
			}
			uci::info_bound(thread, multipv, sc, bd);
		}
		return sc;
	}

	void iterative_deepening(sthread& thread)
	{
		// iterative deepening framework, the base of the search hierarchy
		// starting by generating all root node moves

		board pos{ thread.pos };
		rootpick pick(pos);

		// probing syzygy tablebases
		
		if (thread.use_syzygy = syzygy::tb_cnt > 0; thread.use_syzygy && bit::popcnt(pos.side[both]) <= uci::syzygy.pieces)
		{
			if (pick.tb_pos = syzygy::probe_dtz_root(pos, pick, uci::game_hash); pick.tb_pos)
				thread.use_syzygy = false;
			else
			{
				// using WDL-tables as a fall-back if DTZ-tables are missing
				// allowing probing during the search only if the position is winning

				if (pick.tb_pos = syzygy::probe_wdl_root(pos, pick); pick.tb_pos && !pick.tb_win())
					thread.use_syzygy = false;
			}
		}
		thread.cnt_tbhit += pick.tb_pos;

		// iterative deepening & looping through multi-principal variations

		score sc{ score::none };
		for (depth dt{ 1 }; dt <= uci::limit.dt && !uci::stop; ++dt)
		{
			for (int i{}; i < (int)uci::multipv && i < pick.mv_cnt() && !uci::stop; ++i)
			{
				// rearranging the root move order before starting the alpha-beta search through an aspiration window

				move_var& pv{ thread.pv[i] };
				pick.rearrange_list(pv.mv[0], i > 0 ? thread.pv[i - 1].mv[0] : move{});
				pv.dt = dt;

				try { sc = aspiration_window(thread, pos, pick, dt, i); }
				catch (exception& ex)
				{
					if (ex == exception::stop_search) pick.revert(pos);
					else verify(false);
				}

				// extending the targeted search time if the score is dropping (~7 Elo)

				if (dt >= 4 && sc - pv.sc <= -25)
					thread.chrono.extend(sc - pv.sc);

				// completing the PV

				pv.dt -= uci::stop;
				if (sc != -score::mate)
					pv.sc = sc;
			}

			// providing search information at every iteration & checking the search expiration conditions

			uci::info_iteration(thread, pick.mv_cnt());
			if (expiration::abort(thread.chrono, thread.pv[0].mv, pick, dt, thread.pv[0].sc))
				break;
		}
	}
}

void sthread::start_search()
{
	// entering the search through the search-thread

	verify(uci::mv_offset < rep_hash.size());
	verify(uci::mv_offset < uci::game_hash.size());
	thread_pool::searching += 1;

	// initializing search parameters

	cnt_n = cnt_tbhit = 0;
	for (int i{}; i <= uci::mv_offset; ++i)
		rep_hash[i] = uci::game_hash[i];
	for (depth dt{}; dt < (depth)stack.size(); ++dt)
	{
		stack[dt] = sstack{};
		stack[dt].dt = dt;
	}
	for (auto& cl : counter)
		for (auto& pc : cl)
			for (auto& sq : pc)
				sq = move{};

	// initializing PV related things

	pv.clear();
	pv.resize(uci::multipv);

	// starting the iterative deepening search

	search::iterative_deepening(*this);

	// waiting until all threads have finished their search

	thread_pool::searching -= 1;
	std::unique_lock<std::mutex> lock(thread_pool::mutex);
	if (main())
		while (thread_pool::searching) thread_pool::cv.wait(lock);
	else
		thread_pool::cv.notify_all();
}

void search::start(thread_pool& threads, timemanage::move_time movetime)
{
	// starting point of the search hierarchy

	verify(uci::limit.dt >= 1);
	verify(uci::limit.dt <= lim::dt);

	threads.start_clock(movetime);

	// first resetting the tables for concurrent move-searching (ABDADA)
	// then all search threads are activated and the search begins

	for (auto& slot : abdada::concurrent) for (auto& hash : slot) hash = 0U;

	for (uint32 i{ 0 + 1 }; i < threads.thread.size(); ++i)
		threads.thread[i]->awake();
	threads.thread[0]->start_search();

	// concluding the search and displaying the best move

	bench += threads.thread[0]->get_nodes();
	uci::info_bestmove(threads.get_bestmove());
	uci::stop = true;
}