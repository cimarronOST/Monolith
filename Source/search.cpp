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


#include "move.h"
#include "stream.h"
#include "movepick.h"
#include "utilities.h"
#include "bit.h"
#include "syzygy.h"
#include "movegen.h"
#include "chronos.h"
#include "attack.h"
#include "trans.h"
#include "eval.h"
#include "uci.h"
#include "search.h"

namespace
{
	struct node_count
	{
		int64 fail_high;
		int64 fail_high_1;
		int64 qs;

		int64 total_time;
		int64 total_count;
		int64 total_tbhit;
	} nodes{};
}

namespace
{
	int64 perft(board &pos, int depth, const gen_mode mode)
	{
		// movegen performance test

		if (depth == 0) return 1;

		int64 new_nodes{};
		gen list(pos, mode);
		list.gen_all();
		list.gen_all();
		board saved(pos);

		for (int i{}; i < list.moves; ++i)
		{
			assert_exp(pos.pseudolegal(list.move[i]));

			pos.new_move(list.move[i]);
			assert_exp(mode == PSEUDO || pos.legal());

			if (mode == PSEUDO && !pos.legal())
			{
				pos.revert(saved);
				continue;
			}
			new_nodes += perft(pos, depth - 1, mode);
			pos.revert(saved);
		}
		return new_nodes;
	}
}

// analysis functions for debugging

void analysis::reset()
{
	// resetting analysis-parameters and all node counters

	trans::hash_hits = 0;
	nodes = node_count{};
}

void analysis::summary()
{
	// displaying some summarizing statistics of the performed search(es)

	sync::cout.precision(1);
	sync::cout << std::fixed
		<< "time          : " << nodes.total_time << " ms\n"
		<< "nodes         : " << nodes.total_count << "\n"
		<< "nps           : " << nodes.total_count / std::max(nodes.total_time, 1LL) << " kN/s\n"
		<< "tb hits       : " << nodes.total_tbhit
		<< std::endl;

	if (nodes.total_count > 0)
	{
		sync::cout
			<< "hash hits     : " << trans::hash_hits * 1000 / nodes.total_count / 10.0 << " %\n"
			<< "qs nodes      : " << nodes.qs * 1000 / nodes.total_count / 10.0 << " %"
			<< std::endl;
	}
	if (nodes.fail_high > 0)
	{
		sync::cout
			<< "cutoff move 1 : " << nodes.fail_high_1 * 1000 / nodes.fail_high / 10.0 << " %"
			<< std::endl;
	}
}

void analysis::perft(board &pos, int depth, const gen_mode mode)
{
	// starting perft which is essentially a correctness test of the move-generator

	assert(depth >= 1 && depth <= lim::depth);
	assert(mode == LEGAL || mode == PSEUDO);

	chronometer chrono;
	int64 all_nodes{};
	sync::cout.precision(3);

	for (int d{ 1 }; d <= depth; ++d)
	{
		sync::cout << "perft " << d << ": ";

		auto new_nodes{ ::perft(pos, d, mode) };
		auto time{ chrono.elapsed() };
		all_nodes += new_nodes;

		sync::cout << new_nodes
			<< " time " << time
			<< " nps " << std::fixed << all_nodes / std::max(time, 1LL) << " kN/s"
			<< std::endl;
	}
	nodes.total_count += all_nodes;
	nodes.total_time  += chrono.elapsed();
}

namespace score
{
	// analyzing & adjusting the search score

	bool draw(int score)
	{
		return std::abs(score) == uci::contempt[WHITE];
	}

	bool mate(int score)
	{
		assert(score != SCORE_NONE);
		return std::abs(score) > SCORE_LONGEST_MATE;
	}

	bool longest_mate(int score)
	{
		return SCORE_MATE - score <= uci::limit.mate * 2;
	}

	bool refinable(int score, int hash_score, int bound)
	{
		return hash_score != SCORE_NONE
			&& (bound == EXACT || (bound == LOWER && hash_score > score) || (bound == UPPER && hash_score < score));
	}

	namespace tb
	{
		bool mate(int score)
		{
			return std::abs(score) >= SCORE_TBMATE - lim::depth && !score::mate(score);
		}

		bool refinable(int score, int dtz_score)
		{
			return !(dtz_score == SCORE_TB && score == SCORE_DRAW) && !score::mate(score);
		}

		void adjust(int &score, int tb_score)
		{
			if (score == SCORE_TB) score = tb_score;
			if (score  > SCORE_TB && score < SCORE_LONGEST_MATE) score -= 2 * SCORE_TB;
		}
	}
}

namespace move
{
	// analyzing the move

	bool killer(uint32 move, uint32 counter, uint32 killer[])
	{
		return move == killer[0]
			|| move == killer[1]
			|| move == counter;
	}

	bool extend(uint32 move, bool gives_check, bool pv_node, int depth, const board &pos)
	{
		return (gives_check && (pv_node || depth <= 4))
			|| (pv_node && pos.recapture(move))
			|| (pv_node && move::push_to_7th(move));
	}
}

namespace pv
{
	int get_seldepth(uint32 pv_move[], int seldepth, int depth)
	{
		// retrieving the selective depth, i.e. the highest depth reached

		auto d{ depth };
		while (d < lim::depth - 1 && pv_move[d] != MOVE_NONE)
			d += 1;
		return std::max(seldepth, d);
	}

	void synchronize(sthread &thread, int multipv)
	{
		// synchronizing the thread's provisional PV with its triangular PV-table

		auto &pv{ thread.pv[multipv].move };
		auto &tri_pv{ thread.tri_pv[PREVIOUS] };
		int d{};

		for ( ;     pv[d] != MOVE_NONE; ++d) tri_pv[d] = pv[d];
		for ( ; tri_pv[d] != MOVE_NONE; ++d) tri_pv[d] = MOVE_NONE;
	}

	void prune(sthread &thread, int multipv, board pos)
	{
		// pruning redundant PV-moves

		auto &pv{ thread.pv[multipv] };
		auto draw{ score::draw(pv.score) };
		int d{};
		for ( ; d < lim::depth && pv.move[d] != MOVE_NONE; ++d)
		{
			if (draw && d >= 1)
			{
				auto move_idx{ uci::move_offset + d };
				thread.rep_hash[move_idx] = pos.key;
				if (pos.draw(thread.rep_hash, move_idx))
					break;
			}
			if (!pos.pseudolegal(pv.move[d])) break;
			pos.new_move(pv.move[d]);
			if (!pos.legal()) break;
		}
		for ( ; d < lim::depth && pv.move[d] != MOVE_NONE; ++d)
			pv.move[d] = MOVE_NONE;
	}

	void rearrange(std::vector<move::variation> &pv)
	{
		// sorting the multiple PVs

		std::stable_sort(pv.begin(), pv.end(), [&](move::variation a, move::variation b) { return a.score > b.score; });
	}
}

namespace null
{
	void make_move(board &pos, sthread::sstack *stack)
	{
		// doing a "null move"

		stack->copy = sthread::sstack::null_copy{};
		pos.null_move(stack->copy.ep, stack->copy.capture);
		(stack + 1)->no_pruning = true;
	}

	void revert_move(board &pos, sthread::sstack *stack)
	{
		// reverting the "null move"

		pos.revert_null_move(stack->copy.ep, stack->copy.capture);
		(stack + 1)->no_pruning = false;
	}
}

namespace output
{
	// outputting search information

	std::string score(int score)
	{
		if (score == SCORE_NONE)
			return "";
		return score::mate(score)
			? "mate " + std::to_string((score > SCORE_LONGEST_MATE ? SCORE_MATE + 1 - score : -SCORE_MATE - 1 - score) / 2)
			: "cp "   + std::to_string(score);
	}

	std::string show_multipv(int pv)
	{
		return uci::multipv > 1 ? " multipv " + std::to_string(pv) : "";
	}

	void variation(move::variation &pv)
	{
		if (pv.wrong)
			sync::cout << move::algebraic(pv.move[0]);
		else
		{
			for (int d{}; pv.move[d] != MOVE_NONE; ++d)
				sync::cout << move::algebraic(pv.move[d]) << " ";
		}
	}

	void info(sthread &thread, int move_cnt)
	{
		assert(thread.main);

		if (uci::multipv > 1)
			pv::rearrange(thread.pv);

		auto time { thread.chrono.elapsed() };
		auto nodes{ thread.get_nodes() };

		for (int i{}; i < uci::multipv && i < move_cnt; ++i)
		{
			sync::cout << "info"
				<< " depth " << thread.pv[i].depth
				<< " seldepth " << thread.pv[i].seldepth
				<<   show_multipv(i + 1)
				<< " score " << output::score(thread.pv[i].score)
				<< " time " << time
				<< " nodes " << nodes
				<< " nps " << nodes * 1000 / std::max(time, 1LL)
				<<  (time < 1000 ? "" : " hashfull " + std::to_string(trans::hashfull()))
				<< " tbhits " << thread.get_tbhits()
				<< " pv ";
			variation(thread.pv[i]);
			sync::cout << std::endl;
		}
	}

	void bound_info(sthread &thread, int depth, int multipv, int score, int bound)
	{
		assert(!score::mate(score));
		assert(!score::tb::mate(score));
		assert(bound == UPPER || bound == LOWER);
		auto time { thread.chrono.elapsed() };
		auto nodes{ thread.get_nodes() };

		sync::cout << "info"
			<< " depth " << depth
			<< " seldepth " << pv::get_seldepth(thread.pv[multipv].move, thread.pv[multipv].seldepth, depth)
			<< show_multipv(multipv + 1)
			<< " score cp " << score
			<< (bound == UPPER ? " upperbound" : " lowerbound")
			<< " time " << time
			<< " nodes " << nodes
			<< " nps " << nodes * 1000 / std::max(time, 1LL)
			<< (time < 1000 ? "" : " hashfull " + std::to_string(trans::hashfull()))
			<< std::endl;
	}

	void currmove(sthread &thread, int depth, int multipv, uint32 move, int movenumber)
	{
		if (thread.chrono.elapsed() > 5000)
		{
			std::cout << "info"
				<< " depth " << depth
				<< " seldepth " << pv::get_seldepth(thread.pv[multipv].move, thread.pv[multipv].seldepth, depth)
				<<   show_multipv(multipv + 1)
				<< " currmove " << move::algebraic(move)
				<< " currmovenumber " << movenumber
				<< std::endl;
		}
	}

	void tb_info(int tb_score)
	{
		sync::cout << "info string tablebase ";
		switch (tb_score)
		{
		case -SCORE_TBMATE:       sync::cout << "loss"; break;
		case  SCORE_BLESSED_LOSS: sync::cout << "blessed loss"; break;
		case  SCORE_DRAW:         sync::cout << "draw"; break;
		case  SCORE_CURSED_WIN:   sync::cout << "cursed win"; break;
		case  SCORE_TBMATE:       sync::cout << "win"; break;
		default: assert(false); break;
		}
		sync::cout << std::endl;
	}
}

namespace expiration
{
	// monitoring search expiration

	bool abort(chronometer &chrono, uint32 pv_move[], rootpick &pick, int depth, int score)
	{
		// checking the criteria for early search abortion

		if (uci::infinite)
			return false;
		auto time{ chrono.elapsed() };

		return  time > chrono.max /  2
			|| (time > chrono.max /  8 && score > SCORE_LONGEST_MATE)
			|| (time > chrono.max /  8 && pick.tb_pos)
			|| (time > chrono.max / 16 && depth > 8 && pv_move[depth - 8] == MOVE_NONE)
			|| (time > chrono.max / 32 && pick.single_reply())
			||  score::longest_mate(score);
	}

	bool stop_thread(sthread &thread)
	{
		// keeping track of the elapsing time
		// also increasing the frequency of checking the elapsed time while probing Syzygy-tablebases

		if (++thread.chrono.hits * (1 + 3 * thread.use_syzygy) < 256)
			return false;
		thread.chrono.hits = 0;
		if (uci::infinite)
			return false;
		if (thread.get_nodes() >= uci::limit.nodes)
			return true;

		return thread.chrono.elapsed() >= thread.chrono.max;
	}

	void check(sthread &thread)
	{
		// checking for immediate search termination

		if (uci::stop || stop_thread(thread))
		{
			uci::stop = true;
			throw STOP_SEARCHING;
		}
	}
}

namespace update
{
	void add_history(sthread &thread, uint32 move, int weight, int turn)
	{
		// updating the history table

		assert(turn == move::turn(move));
		
		auto &history{ thread.history[turn][move::piece(move)][move::sq2(move)] };
		history += weight;
		assert(history < sort::history_max);
	}

	void history_stats(sthread &thread, uint32 move, uint32 quiet_move[], int quiet_count, int depth, int turn)
	{
		// updating history table if a quiet move fails high

		add_history(thread, move, depth * depth, turn);
		for (int i{}; i < quiet_count - 1; ++i)
			add_history(thread, quiet_move[i], -depth, turn);
	}

	void killer_stats(uint32 killer[], uint32 move)
	{
		// updating killer-moves if a quiet move fails high

		if (move != killer[0])
		{
			killer[1] = killer[0];
			killer[0] = move;
		}
	}

	void triangular_pv(uint32 move, uint32 tri_pv[][lim::depth], int curr_depth)
	{
		// saving the partial pv if a new best move has been found

		tri_pv[curr_depth - 1][0] = move;
		for (int d{}; tri_pv[curr_depth][d] != MOVE_NONE; ++d)
			tri_pv[curr_depth - 1][d + 1] = tri_pv[curr_depth][d];
	}

	void main_pv(uint32 move, uint32 pv_move[], uint32 tri_pv[][lim::depth])
	{
		// updating the whole pv if a new best root move has been found

		pv_move[0] = move;
		for (int d{}; tri_pv[MAIN][d] != MOVE_NONE; ++d)
			pv_move[d + 1] = tri_pv[MAIN][d];
	}
}

namespace abdada
{
	// simplified ABDADA
	// all credits for the algorithm go to Tom Kerrigan:
	// http://www.tckerrigan.com/Chess/Parallel_Search/Simplified_ABDADA

	constexpr int depth_defer { 3 };
	constexpr int depth_cutoff{ 4 };
	constexpr int slots       { 4 };
	constexpr uint32 size{ 1U << 15 };
	constexpr uint32 mask{ size - 1 };
	uint32 concurrent[size][slots]{};

	uint32 to_key(uint32 move, uint64 &pos_key)
	{
		// generating a move-hash-key

		return static_cast<uint32>(pos_key) ^ (move * 1664525U + 1013904223U);
	}

	bool defer_move(uint32 move_hash)
	{
		// checking if a move is already being searched by another thread

		auto &entry{ concurrent[move_hash & mask] };
		return std::any_of(entry, entry + slots, [&](uint32 &move) { return move == move_hash; });
	}

	void add_move(uint32 move_hash)
	{
		// adding a move to the currently-searching-list

		auto &all_slots{ concurrent[move_hash & mask] };
		for (auto &entry : all_slots)
		{
			if (entry == 0U)
			{
				entry = move_hash;
				return;
			}
			if (entry == move_hash)
				return;
		}
		all_slots[0] = move_hash;
	}

	void remove_move(uint32 move_hash)
	{
		// removing a move from the currently-searching-list

		auto &all_slots{ concurrent[move_hash & mask] };
		for (auto &entry : all_slots)
		{
			if (entry == move_hash)
				entry = 0;
		}
	}
}

void search::reset()
{
	// resetting the tables for concurrent move-searching (ABDADA)

	for (auto &slot : abdada::concurrent) for (auto &hash : slot) hash = 0U;
}

namespace search
{
	int qsearch(sthread &thread, board &pos, sthread::sstack *stack, int depth, int alpha, int beta)
	{
		// quiescence search at the leaf nodes

		assert(-SCORE_MATE <= alpha && alpha < beta && beta <= SCORE_MATE);
		assert(depth <= 0);
		assert_exp(depth != 0 || !pos.check());

		// detecting draws

		auto offset{ uci::move_offset + stack->depth };
		thread.rep_hash[offset] = pos.key;
		if (pos.draw(thread.rep_hash, offset))
			return uci::contempt[pos.xturn];

		expiration::check(thread);

		// transposition table lookup

		trans::entry tt{};
		if (depth == 0 && trans::probe(pos.key, tt, depth, stack->depth))
		{
			if (tt.bound == EXACT
			|| (tt.bound == LOWER && tt.score >= beta)
			|| (tt.bound == UPPER && tt.score <= alpha))
				return tt.score;
		}

		// standing pat & considering checks

		auto stand_pat{ eval::static_eval(pos, thread.pawnhash) };
		auto best_score{ stand_pat };
		auto in_check{ depth == -1 && pos.check() };

		if (!in_check && stand_pat > alpha)
		{
			if (stand_pat >= beta)
				return stand_pat;
			alpha = stand_pat;
		}

		// generating and sorting moves while looping through them

		movepick pick(pos, in_check, depth);
		for (uint32 move{ pick.next() }; move != MOVE_NONE; move = pick.next())
		{
			assert(depth >= -1 || !move::quiet(move));
			assert_exp(pos.pseudolegal(move));
			auto skip_pruning{ in_check || move::quiet(move) };

			// depth limit pruning

			if (!skip_pruning
				&& depth <= -6
				&& !pos.recapture(move))
				continue;

			// delta pruning

			if (!skip_pruning
				&& !pos.lone_king()
				&& !move::promo(move)
				&& stand_pat + attack::value[move::victim(move)] + 100 < alpha)
				continue;

			// SEE pruning

			if (!skip_pruning
				&& !move::promo(move)
				&& attack::value[move::piece(move)] > attack::value[move::victim(move)]
				&& attack::see(pos, move) < 0)
				continue;

			pos.new_move(move);
			assert_exp(pos.legal());
			thread.nodes += 1;
			thread.count.qs += 1;

			auto score{ -qsearch(thread, pos, stack + 1, depth - 1, -beta, -alpha) };
			pos.revert(pick.list.pos);
			assert(score != SCORE_NONE);

			if (score > best_score)
			{
				best_score = score;
				if (score > alpha)
				{
					alpha = score;
					if (score >= beta)
						return score;
				}
			}
		}

		// detecting checkmate

		if (in_check && pick.hits == 0)
			return stack->depth - SCORE_MATE;

		assert(-SCORE_MATE < best_score && best_score < SCORE_MATE);
		return best_score;
	}

	int alphabeta(sthread &thread, board &pos, sthread::sstack *stack, int depth, int alpha, int beta)
	{
		// main alpha-beta search
		// first detecting draws or dropping into quiescence search at leaf nodes

		assert(-SCORE_MATE <= alpha && alpha < beta && beta <= SCORE_MATE);
		assert(depth <= lim::depth);

		auto offset{ uci::move_offset + stack->depth };
		thread.rep_hash[offset] = pos.key;

		if (pos.draw(thread.rep_hash, offset))
			return uci::contempt[pos.xturn];

		if (depth <= 0 || stack->depth >= lim::depth)
			return qsearch(thread, pos, stack, 0, alpha, beta);

		expiration::check(thread);

		// mate distance pruning

		auto a_bound{ alpha < -SCORE_MATE + stack->depth ? -SCORE_MATE + stack->depth : alpha };
		auto b_bound{  beta >  SCORE_MATE - stack->depth ?  SCORE_MATE - stack->depth :  beta };
		if  (b_bound <= a_bound)
			return a_bound;

		// probing transposition table

		bool pv_node{ beta != alpha + 1 };
		auto key{ pos.key ^ static_cast<uint64>(stack->skip_move) };
		trans::entry tt{};
		if (trans::probe(key, tt, depth, stack->depth))
		{
			if (!pv_node)
				tt.move = MOVE_NONE;

			if (tt.score <= alpha || tt.score >= beta)
			{
				if (pv_node)
					tt.move = MOVE_NONE;

				else if (tt.bound == EXACT
					||  (tt.bound == LOWER && tt.score >= beta)
					||  (tt.bound == UPPER && tt.score <= alpha))
					return tt.score;
			}
		}

		// probing syzygy tablebases

		if (thread.use_syzygy)
		{
			auto piece_cnt{ bit::popcnt(pos.side[BOTH]) };
			if (piece_cnt <= uci::syzygy.pieces
				&& (piece_cnt < std::min(5, uci::syzygy.pieces) || depth >= uci::syzygy.depth)
				&& pos.half_count == 0)
			{
				int success{};
				int score{ SCORE_NONE };
				int wdl{ syzygy::probe_wdl(pos, success) };
				if (success)
				{
					if (wdl < -1 || (wdl < 0 && !uci::syzygy.rule50))
						score = -SCORE_TBMATE + stack->depth;
					else if (wdl > 1 || (wdl > 0 && !uci::syzygy.rule50))
						score =  SCORE_TBMATE - stack->depth;
					else
						score = uci::contempt[pos.xturn] + wdl;

					thread.count.tbhit += 1;
					return score;
				}
			}
		}

		// initializing pruning & evaluating the current position

		auto in_check{ pos.check() };
		auto crucial_node{ pv_node || in_check };
		auto skip_pruning{ crucial_node || stack->no_pruning };
		auto score{ crucial_node ? SCORE_NONE : eval::static_eval(pos, thread.pawnhash) };

		if (score::refinable(score, tt.score, tt.bound))
			score = tt.score;

		// static null move pruning

		if (depth <= 3 && !score::mate(beta) && !skip_pruning && score - depth * 50 >= beta)
			return beta;

		// razoring

		if (depth <= 3 && !skip_pruning && score + depth * 50 + 100 <= alpha)
		{
			auto raz_alpha{ alpha - depth * 50 - 100 };
			auto new_score{ qsearch(thread, pos, stack, 0, raz_alpha, raz_alpha + 1) };
			if (new_score <= raz_alpha)
				return alpha;
		}

		// null move pruning

		if (depth >= 2 && !skip_pruning && !stack->skip_move && !pos.lone_king() && score >= beta)
		{
			auto R{ 3 + std::min(3, (score - beta) / 128) };
			null::make_move(pos, stack);
			thread.nodes += 1;
			auto null_score{ -alphabeta(thread, pos, stack + 1, depth - 1 - R, -beta, 1 - beta) };
			null::revert_move(pos, stack);

			if (null_score >= beta)
				return beta;
		}

		// forcing the previous PV-move to the top of the movelist
		// the move will be checked for pseudo-legality by the move-generator afterwards

		if (thread.tri_pv[PREVIOUS][stack->depth])
		{
			tt = { thread.tri_pv[PREVIOUS][stack->depth], SCORE_NONE, 0, 0 };
			thread.tri_pv[PREVIOUS][stack->depth] = MOVE_NONE;
		}

		// internal iterative deepening

		else if (pv_node && tt.move == MOVE_NONE && !stack->no_pruning && depth >= 3)
		{
			stack->no_pruning = true;
			alphabeta(thread, pos, stack, depth - 2, alpha, beta);
			stack->no_pruning = false;
			trans::probe(key, tt, depth, stack->depth);
		}

		// initializing move loop
		
		auto &counter{ thread.counter_move[move::piece((stack - 1)->move)][move::sq2((stack - 1)->move)] };
		auto futile{ !crucial_node && depth <= 6 };
		auto fut_score{ score + 50 + 100 * depth };
		auto old_alpha{ alpha };
		auto best_score{ -SCORE_MATE };
		uint32 best_move{ MOVE_NONE };
		
		uint32 defer_move[lim::moves]{};
		uint32 quiet_move[lim::moves]{};
		int defer_count{};
		int quiet_count{};
		
		// generating and sorting moves while looping through them

		movepick pick(pos, tt.move, counter, stack->killer, thread.history, defer_move);
		for (uint32 move{ pick.next() }; move != MOVE_NONE; move = pick.next())
		{
			assert(pick.hits >= 1 && pick.hits <= lim::moves);
			assert(pick.hits == 1 || move != tt.move || uci::thread_count > 1);

			// cutoff check (ABDADA)

			if (defer_count > 0 && !pv_node && depth >= abdada::depth_cutoff && pick.can_defer())
			{
				assert(uci::thread_count > 1);
				if (trans::probe(key, tt, depth, stack->depth) && (tt.bound == LOWER && tt.score >= beta))
					return tt.score;
			}

			// initializing

			if (move == stack->skip_move)
			{
				pick.hits -= 1;
				continue;
			}

			auto gives_check{ pos.gives_check(move) };
			auto quiet{ move::quiet(move) };
			auto dangerous{ pick.hits == 1
				|| gives_check
				|| !quiet
				|| move::pawn_advance(move)
				|| move::castling(move)
				|| move::killer(move, counter, stack->killer) };

			assert_exp(pos.pseudolegal(move));
			thread.nodes += 1;
			if (quiet) quiet_move[quiet_count++] = move;

			// futility pruning

			if (futile && !dangerous && fut_score <= alpha && !score::mate(alpha) && !score::tb::mate(alpha))
				continue;

			// late move pruning

			if (!skip_pruning && !dangerous && depth <= 3 && pick.hits >= depth * 4)
				continue;

			// SEE pruning

			if (!skip_pruning && !dangerous && depth <= 4 && attack::see(pos, move) < 0)
				continue;

			// deferring moves that are searched by other threads (ABDADA)

			auto move_hash{ abdada::to_key(move, pos.key) };
			if (pick.hits > 1 && pick.can_defer() && depth >= abdada::depth_defer && abdada::defer_move(move_hash))
			{
				defer_move[defer_count++] = move;
				thread.nodes -= 1;
				continue;
			}

			// singular extension

			int extension{};
			if (move == tt.move && depth >= 6 && tt.bound == LOWER && tt.score != SCORE_NONE && tt.depth >= depth - 4)
			{
				assert(pick.hits == 1 || uci::thread_count > 1);

				auto alpha_bound{ std::max(tt.score - 2 * depth, -SCORE_MATE) };
				stack->skip_move = move;
				score = -alphabeta(thread, pos, stack, depth - 4, alpha_bound, alpha_bound + 1);
				assert(stack->skip_move == move);
				stack->skip_move = MOVE_NONE;

				if (score <= alpha_bound)
					extension = 1;
			}

			// other extensions

			if (!extension && move::extend(move, gives_check, pv_node, depth, pick.list.pos))
				 extension = 1;

			// doing the move and checking if it is legal

			pos.new_move(move);
			assert_exp(gives_check == pos.check());
			if (!pos.legal())
			{
				pick.revert(pos); continue;
			}
			stack->move = move;

			// late move reduction

			int reduction{};
			if (!extension && depth >= 3 && pick.hits >= 4 && !in_check && !gives_check && quiet && !move::pawn_advance(move))
			{
				assert(pos.xturn == move::turn(move));
				auto history{ thread.history[pos.xturn][move::piece(move)][move::sq2(move)] };

				reduction = 1 + (pick.hits - 4) / 9 + (depth - 3) / 4 - pv_node;
				reduction = value::minmax(reduction + history / -5000, 0, 6);
			}
			
			// late move reduction search

			auto new_depth{ depth - 1 + extension };
			auto new_alpha{ pv_node && pick.hits > 1 ? -alpha - 1 : -beta };
			if (reduction)
				score = -alphabeta(thread, pos, stack + 1, new_depth - reduction, -alpha - 1, -alpha);

			// principal variation search (with ABDADA and LMR-research)

			if (!reduction || (reduction && score > alpha))
			{
				if (pick.hits > 1 && pick.can_defer() && depth > abdada::depth_defer)
				{
					abdada::add_move(move_hash);
					score = -alphabeta(thread, pos, stack + 1, new_depth, new_alpha, -alpha);
					abdada::remove_move(move_hash);
				}
				else score = -alphabeta(thread, pos, stack + 1, new_depth, new_alpha, -alpha);

				if (pick.hits > 1 && pv_node && score > alpha)
					score = -alphabeta(thread, pos, stack + 1, new_depth, -beta, -alpha);
			}

			pos.revert(pick.list.pos);
			assert(score != SCORE_NONE);

			// checking for a new best move

			if (score > best_score)
			{
				best_score = score;
				if (score > alpha)
				{
					// checking for a beta cutoff
					
					best_move  = move;
					if (score >= beta)
					{
						thread.count.fail_high   += 1;
						thread.count.fail_high_1 += (pick.hits == 1);
						if (quiet)
						{
							update::history_stats(thread, move, quiet_move, quiet_count, depth, pos.turn);
							update::killer_stats(stack->killer, move);
							counter = move;
						}
						break;
					}
					alpha = score;
					update::triangular_pv(move, thread.tri_pv, stack->depth);
				}
			}
		}

		// detecting checkmate & stalemate

		if (pick.hits == 0)
		{
			assert(alpha == old_alpha);
			return stack->skip_move ? alpha : (in_check ? stack->depth - SCORE_MATE : uci::contempt[pos.xturn]);
		}
		
		// storing the results in the transposition table

		if (!stack->skip_move)
		{
			trans::store(key, best_move, best_score,
				best_score <= old_alpha ? UPPER : (best_score >= beta ? LOWER : EXACT), depth, stack->depth);
		}

		assert(-SCORE_MATE < best_score && best_score < SCORE_MATE);
		return best_score;
	}

	int alphabeta_root(sthread &thread, board &pos, rootpick &pick, int depth, int alpha, int beta, int multipv)
	{
		// starting the alpha-beta search with the root nodes

		assert(1 <= depth && depth <= lim::depth);
		assert(-SCORE_MATE <= alpha && alpha < beta && beta <= SCORE_MATE);

		int score{ SCORE_NONE };
		bool wrong_pv{};
		auto stack{ thread.stack };

		// looping through the movelist

		for (int i{}, move_count{}; i < pick.list.moves; ++i)
		{
			auto &node{ pick.sort.root[i] };
			if (node.skip) continue;
			move_count += 1;

			assert_exp(pos.pseudolegal(node.move));
			assert(pick.list.find(node.move));

			if (thread.main)
				output::currmove(thread, depth, multipv, node.move, move_count);
			node.nodes -= thread.nodes;

			pos.new_move(node.move);
			thread.nodes += 1;
			assert_exp(pos.legal());
			stack->move = node.move;

			// check extension & PVS

			int extension{ node.check };
			auto new_depth{ depth - 1 + extension };
			auto new_alpha{ move_count > 1 ? -alpha - 1 : -beta };

			score = -alphabeta(thread, pos, stack + 1, new_depth, new_alpha, -alpha);
			if (move_count > 1 && score > alpha)
				score = -alphabeta(thread, pos, stack + 1, new_depth, -beta, -alpha);

			node.nodes += thread.nodes;
			pos.revert(pick.list.pos);

			assert(node.nodes >= 0);
			assert(score > -SCORE_MATE && score < SCORE_MATE);
			assert(score != SCORE_NONE);

			// refining the search scores with dtz scores if the root node is a tb-position

			if (pick.tb_pos)
			{
				if (score::draw(score)) score = SCORE_DRAW;
				auto dtz_score{ static_cast<int>(node.weight) };

				wrong_pv = score::tb::refinable(score, dtz_score);
				if (wrong_pv || (dtz_score == SCORE_TB && !score::mate(score)))
					score = dtz_score;
			}

			if (score > alpha)
			{
				if (score >= beta)
					return score;

				alpha = score;
				update::main_pv(node.move, thread.pv[multipv].move, thread.tri_pv);
				node.nodes += thread.nodes;

				if (pick.tb_pos)
					thread.pv[multipv].wrong = wrong_pv;
			}
		}
		return alpha;
	}

	int aspiration_window(sthread &thread, board &pos, rootpick &pick, int depth, int multipv)
	{
		// entering the alpha-beta-search through an aspiration window
		// the window widens dynamically depending on the bound the search returns

		int alpha{ -SCORE_MATE }, beta{ SCORE_MATE };
		int bound{ EXACT };
		int score{ SCORE_NONE };
		int score_old{ thread.pv[multipv].score };
		int lower_margin{ 35 }, upper_margin{ 35 };
		
		assert(depth == 1 || score_old != SCORE_NONE);

		while (true)
		{
			if (depth >= 4)
			{
				alpha = std::max(score_old - lower_margin, -(int)SCORE_MATE);
				beta  = std::min(score_old + upper_margin,  (int)SCORE_MATE);
				assert(alpha < beta);
			}

			score = alphabeta_root(thread, pos, pick, depth, alpha, beta, multipv);
			assert(score != SCORE_NONE);

			if (score <= alpha)
			{
				bound = UPPER;
				lower_margin *= 4;
			}
			else if (score >= beta)
			{
				bound = LOWER;
				upper_margin *= 4;
			}

			if ((alpha < score && score < beta) || score::mate(score) || score::tb::mate(score))
				break;

			if (thread.main)
				output::bound_info(thread, depth, multipv, score, bound);
		}
		return score;
	}

	void iterative_deepening(sthread &thread)
	{
		// iterative deepening framework, the base of the search hierarchy

		int score{ SCORE_NONE };
		int tb_score{ SCORE_NONE };

		// generating root node moves

		board pos{ thread.pos };
		rootpick pick(pos);

		// probing syzygy tablebases

		thread.use_syzygy = { syzygy::tablebases > 0 };
		if (thread.use_syzygy && bit::popcnt(pos.side[BOTH]) <= uci::syzygy.pieces)
		{
			pick.tb_pos = syzygy::probe_dtz_root(pos, pick, uci::quiet_hash, tb_score);

			if (pick.tb_pos)
				thread.use_syzygy = false;
			else
			{
				// using WDL-tables as a fall-back if DTZ-tables are missing
				// allowing probing during the search only if the position is winning

				pick.tb_pos = syzygy::probe_wdl_root(pos, pick, tb_score);
				if (tb_score <= SCORE_DRAW)
					thread.use_syzygy = false;
			}
		}
		thread.count.tbhit += pick.tb_pos;

		// iterative deepening & looping through multi-principal variations

		for (int depth{ 1 }; depth <= uci::limit.depth && !uci::stop; ++depth)
		{
			for (int i{}; i < uci::multipv && i < pick.list.moves && !uci::stop; ++i)
			{
				// rearranging root moves & synchronizing the PV from the last iteration

				pick.rearrange_moves(thread.pv[i].move[0], i > 0
					? thread.pv[i - 1].move[0]
					: uint32(MOVE_NONE));
				pv::synchronize(thread, i);

				// starting alpha-beta search through an aspiration window

				try { score = aspiration_window(thread, pos, pick, depth, i); }
				catch (exception_type &ex)
				{
					if (ex == STOP_SEARCHING) pos.revert(pick.list.pos);
					else assert(false);
				}

				// completing the PV

				thread.pv[i].depth = depth - uci::stop;
				thread.pv[i].seldepth = pv::get_seldepth(thread.pv[i].move, thread.pv[i].seldepth, depth);

				if (score != -SCORE_MATE)
				{
					if (pick.tb_pos)
						score::tb::adjust(score, tb_score);

					thread.pv[i].score = score;
					pv::prune(thread, i, pos);
				}
			}

			// providing search information at every iteration

			if (thread.main)
				output::info(thread, pick.list.moves);

			if (expiration::abort(thread.chrono, thread.pv[MAIN].move, pick, depth, thread.pv[MAIN].score))
				break;
		}

		// concluding the search

		if (thread.main && uci::multipv == 1 && thread.pv[MAIN].wrong)
			output::tb_info(tb_score);
	}
}

void sthread::start_search()
{
	// entering the search through the search-thread

	thread_pool::searching += 1;

	// initializing search parameters

	this->nodes = 0LL;
	count = node_count{};

	for (int d{}; d <= lim::depth; ++d)                 { stack[d] = sstack{}; stack[d].depth = d; }
	for (int i{}; i <= uci::move_offset; ++i)             rep_hash[i] = uci::quiet_hash[i];
	for (auto &piece : counter_move)                      for (auto &sq : piece) sq = MOVE_NONE;
	for (auto &color : history) for (auto &piece : color) for (auto &sq : piece) sq = 0LL;

	// initializing PV related things

	pv.clear();
	pv.resize(uci::multipv);
	for (auto &d : tri_pv) for (auto &move : d) move = MOVE_NONE;

	// starting the iterative deepening search

	search::iterative_deepening(*this);

	// waiting until all threads have finished their search

	thread_pool::searching -= 1;
	std::unique_lock<std::mutex> lock(thread_pool::mutex);
	if (main)
		while (thread_pool::searching) thread_pool::cv.wait(lock);
	else
		thread_pool::cv.notify_all();
}

void search::start(thread_pool &threads, int64 movetime)
{
	// starting point of the search hierarchy

	assert(uci::limit.depth >= 1);
	assert(uci::limit.depth <= lim::depth);

	threads.start_clock(movetime);

	// starting all search threads

	for (uint32 i{ MAIN + 1 }; i < threads.thread.size(); ++i)
		threads.thread[i]->awake();
	threads.thread[MAIN]->start_search();

	// synchronizing threads if the main thread had't concluded at least the first iteration
	// this is done to always try to provide a best-move & ponder-move

	uint32 &bestmove{ threads.thread[MAIN]->pv[MAIN].move[0] };
	uint32 &ponder  { threads.thread[MAIN]->pv[MAIN].move[1] };
	if (!bestmove)
	{
		for (uint32 i{ MAIN + 1 }; i < threads.thread.size(); ++i)
		{
			if (threads.thread[i]->pv[MAIN].move[0])
			{
				bestmove = threads.thread[i]->pv[MAIN].move[0];
				ponder   = threads.thread[i]->pv[MAIN].move[1];
				break;
			}
		}
	}

	// adding up some counters after the search for analysis purpose

	nodes.total_time  += threads.thread[MAIN]->chrono.elapsed();
	nodes.total_count += threads.thread[MAIN]->get_nodes();
	nodes.total_tbhit += threads.thread[MAIN]->get_tbhits();

	for (auto t : threads.thread)
	{
		nodes.fail_high   += t->count.fail_high;
		nodes.fail_high_1 += t->count.fail_high_1;
		nodes.qs          += t->count.qs;
	}

	// displaying best move & ponder move

	sync::cout << "bestmove " <<  move::algebraic(bestmove)
		<< (ponder ? " ponder " + move::algebraic(ponder) : "") << std::endl;
	uci::stop = true;
}