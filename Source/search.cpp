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


#include "movegen.h"
#include "chronos.h"
#include "move.h"
#include "notation.h"
#include "attack.h"
#include "see.h"
#include "trans.h"
#include "eval.h"
#include "engine.h"
#include "search.h"

namespace
{
	struct node_count
	{
		uint64 cnt{ };
		uint64 fail_high{ };
		uint64 fail_high_1st{ };
		uint64 total{ };
		uint64 qs{ };
	} nodes;

	uint64 searchtime{ };
}

namespace position
{
	bool legal(board &pos)
	{
		return attack::check(pos, pos.xturn, pos.pieces[KINGS] & pos.side[pos.xturn]);
	}
}

// analysis functions for debugging

void analysis::reset()
{
	trans::hash_hits = 0;
	searchtime = 0ULL;

	nodes.fail_high = 0;
	nodes.fail_high_1st = 0;
	nodes.total = 0;
	nodes.qs = 0;
}

void analysis::summary()
{
	sync::cout.precision(1);
	sync::cout << std::fixed
		<< "time            : " << searchtime << " ms\n"
		<< "nodes           : " << nodes.total << "\n"
		<< "nps             : " << nodes.total / std::max(searchtime, 1ULL) << " kN/s"
		<< std::endl;

	if (nodes.total > 0)
	{
		sync::cout
			<< "hash hits       : " << trans::hash_hits * 1000 / nodes.total / 10.0 << " %\n"
			<< "qs nodes        : " << nodes.qs * 1000 / nodes.total / 10.0 << " %"
			<< std::endl;
	}
	if (nodes.fail_high > 0)
	{
		sync::cout
			<< "cutoff 1st move : " << nodes.fail_high_1st * 1000 / nodes.fail_high / 10.0 << " %"
			<< std::endl;
	}
}

void analysis::root_perft(board &pos, int depth, const gen_mode mode)
{
	// starting the movegen performance test

	assert(depth >= 1 && depth <= lim::depth);
	assert(mode == LEGAL || mode == PSEUDO);

	chronometer chrono;
	nodes.cnt = 0;
	sync::cout.precision(3);

	for (auto d{ 1 }; d <= depth; ++d)
	{
		sync::cout << "perft " << d << ": ";

		auto new_nodes{ perft(pos, d, mode) };
		chrono.split();
		nodes.cnt += new_nodes;

		sync::cout << new_nodes
			<< " time " << chrono.elapsed
			<< " nps " << std::fixed << nodes.cnt / std::max(chrono.elapsed, 1ULL) << " kN/s"
			<< std::endl;
	}
	nodes.total += nodes.cnt;
	searchtime += chrono.split();
}

uint64 analysis::perft(board &pos, int depth, const gen_mode mode)
{
	if (depth == 0) return 1;

	uint64 new_nodes{};
	gen list(pos, mode);
	list.gen_all();
	board saved(pos);

	for (auto i{ 0 }; i < list.cnt.moves; ++i)
	{
		assert_exp(pos.pseudolegal(list.move[i]));

		pos.new_move(list.move[i]);
		assert_exp(mode == PSEUDO || position::legal(pos));

		if (mode == PSEUDO && !position::legal(pos))
		{
			pos.revert(saved);
			continue;
		}
		new_nodes += perft(pos, depth - 1, mode);
		pos.revert(saved);
	}
	return new_nodes;
}

// actual search

namespace
{
	// triangular pv table & position hash table & time management

	struct evolving_pv{ uint32 pv[lim::depth][lim::depth]{ }; } evolving;
	uint64 hash[256 + lim::depth]{ };
	chronometer chrono;

	// move ordering tables

	sort::kill_list killer{ };
	sort::hist_list history{ };

	uint32 counter_move[6][64]{ };
}

namespace init
{
	void search()
	{
		// resetting search parameters

		nodes.cnt = 0;
		for (auto i{ 0 }; i <= engine::move_offset; ++i)
			hash[i] = engine::quiet_hash[i];

		for (auto &i : evolving.pv) for (auto &j : i) j = NO_MOVE;
		for (auto &i : counter_move) for (auto &j : i) j = NO_MOVE;
		for (auto &i : killer.list) for (auto &j : i) j = NO_MOVE;
		for (auto &i : history.list) for (auto &j : i) for (auto &k : j) k = 1000ULL;
	}
}

namespace score
{
	bool mate(int score)
	{
		return abs(score) > MATE_SCORE;
	}

	bool mate_limit(int score)
	{
		return MAX_SCORE - score <= engine::limit.mate * 2;
	}

	bool refineable(int score, int hash_score, int bound)
	{
		return hash_score != NO_SCORE
			&& (bound == EXACT || (bound == LOWER && hash_score > score) || (bound == UPPER && hash_score < score));
	}
}

namespace move
{
	bool is_killer(uint32 move, uint32 counter, int curr_depth)
	{
		return move == killer.list[curr_depth][0]
			|| move == killer.list[curr_depth][1]
			|| move == counter;
	}

	bool extend(uint32 move, bool gives_check, bool pv_node, int depth, const board &pos)
	{
		return (gives_check && (pv_node || depth <= 4))
			|| (pv_node && pos.recapture(move))
			|| (pv_node && move::is_push_to_7th(move));
	}
}

namespace pv
{
	int get_seldepth(uint32 pv_move[], int seldepth, int depth)
	{
		// retrieving the selective depth

		auto d{ depth };
		while (d < lim::depth - 1 && pv_move[d] != NO_MOVE)
			d += 1;
		return std::max(seldepth, d);
	}

	void prune(search::variation &pv, board pos)
	{
		// pruning wrong principal variation entries

		for (pv.maxdepth = 0; pv.maxdepth < lim::depth && pv.move[pv.maxdepth] != NO_MOVE; pv.maxdepth += 1)
		{
			if (!pos.pseudolegal(pv.move[pv.maxdepth]))
				break;

			pos.new_move(pv.move[pv.maxdepth]);
			if (!position::legal(pos))
				break;	
		}
		for (auto d{ pv.maxdepth }; d < lim::depth && pv.move[d] != NO_MOVE; pv.move[d++] = NO_MOVE);
	}

	void store(search::variation &pv, board pos)
	{
		// storign the principal variation in the transposition table

		assert(pv.score != NO_SCORE);
		auto score{ pv.score };

		for (auto d{ 0 }; d < lim::depth && pv.move[d] != NO_MOVE; ++d, score = -score)
		{
			assert(pv.maxdepth - d > 0);

			trans::store(pos, pv.move[d], score, EXACT, 0, d);
			pos.new_move(pv.move[d]);
			assert(position::legal(pos));
		}
	}

	void rearrange(std::vector<search::variation> &pv)
	{
		// sorting multi principal variations

		std::stable_sort(pv.begin(), pv.end(), [&](search::variation a, search::variation b) { return a.score > b.score; });
	}
}

namespace null
{
	void make_move(board &pos, search::search_stack *stack)
	{
		stack->copy.ep = 0ULL;
		stack->copy.capture = 0;
		pos.null_move(stack->copy.ep, stack->copy.capture);
		(stack + 1)->no_pruning = true;
	}

	void revert_move(board &pos, search::search_stack *stack)
	{
		pos.revert_null_move(stack->copy.ep, stack->copy.capture);
		(stack + 1)->no_pruning = false;
	}
}

namespace output
{
	// outputting search information

	std::string score(int score)
	{
		return score::mate(score)
			? "mate " + std::to_string((score > MATE_SCORE ? MAX_SCORE + 1 - score : MIN_SCORE - 1 - score) / 2)
			: "cp " + std::to_string(score);
	}

	std::string show_multipv(int pv)
	{
		if (engine::multipv > 1)
			return " multipv " + std::to_string(pv);
		else
			return "";
	}

	void variation(uint32 pv_move[])
	{
		for (auto d{ 0 }; pv_move[d] != NO_MOVE; ++d)
			sync::cout << notation::algebraic(pv_move[d]) << " ";
	}

	void info(std::vector<search::variation> &pv)
	{
		if (engine::multipv > 1)
			pv::rearrange(pv);

		for (auto idx{ 0 }; idx < engine::multipv; ++idx)
		{
			sync::cout << "info"
				<< " depth " << pv[idx].mindepth
				<< " seldepth " << pv[idx].seldepth
				<<   show_multipv(idx + 1)
				<< " score " << score(pv[idx].score)
				<< " time " << chrono.elapsed
				<< " nodes " << nodes.cnt
				<< " nps " << nodes.cnt * 1000 / std::max(chrono.elapsed, 1ULL)
				<< " hashfull " << trans::hashfull()
				<< " pv "; variation(pv[idx].move);

			sync::cout << std::endl;
		}
	}

	void bound_info(search::variation &pv, int depth, int multipv, int score, int bound)
	{
		assert(!score::mate(score));
		chrono.split();

		sync::cout << "info"
			<< " depth " << depth
			<< " seldepth " << pv::get_seldepth(pv.move, pv.seldepth, depth)
			<<   show_multipv(multipv)
			<< " score cp " << score
			<< (bound == UPPER ? " upper" : " lower") << "bound"
			<< " time " << chrono.elapsed
			<< " nodes " << nodes.cnt
			<< " nps " << nodes.cnt * 1000 / std::max(chrono.elapsed, 1ULL)
			<< std::endl;
	}

	void currmove(int depth, int multipv, uint32 move, int movenumber)
	{
		chrono.split();

		if ((chrono.elapsed > 1000 && movenumber <= 3) || chrono.elapsed > 5000)
		{
			std::cout << "info depth " << depth
				<<   show_multipv(multipv)
				<< " currmove " << notation::algebraic(move)
				<< " currmovenumber " << movenumber
				<< " time " << chrono.elapsed
				<< std::endl;
		}
	}

}

namespace expire
{
	// monitoring search expiration

	void start_clock(uint64 movetime)
	{
		chrono.hits = 0;
		chrono.max = movetime;
		chrono.start();
	}

	bool abort(int depth, search::variation &pv, movepick_root &pick)
	{
		// checking the criteria for early search abortion

		if (engine::infinite)
			return false;

		return (chrono.elapsed > chrono.max / 2)
			|| (pv.score > MATE_SCORE && chrono.elapsed > chrono.max / 8)
			|| (depth > 8 && pv.move[depth - 8] == NO_MOVE && chrono.elapsed > chrono.max / 16)
			|| (pick.single_reply() && chrono.elapsed > chrono.max / 32)
			|| (engine::limit.mate && score::mate_limit(pv.score));
	}

	bool stop_thread()
	{
		if (++chrono.hits < 256)
			return false;
		chrono.hits = 0;
		if (engine::infinite)
			return false;
		if (nodes.cnt >= engine::limit.nodes)
			return true;

		return chrono.split() >= chrono.max;
	}

	void check()
	{
		if (engine::stop || stop_thread())
		{
			engine::stop = true;
			throw 1;
		}
	}
}

namespace update
{
	// updating during search

	void kill_table(uint32 move, int curr_depth)
	{
		assert(curr_depth < lim::depth);

		if (move == killer.list[curr_depth][0])
			return;

		killer.list[curr_depth][1] = killer.list[curr_depth][0];
		killer.list[curr_depth][0] = move;
	}

	void hist_table(const board &pos, uint32 move, int depth)
	{
		assert(pos.turn == move::turn(move));

		uint64 *entry{ &(history.list[pos.turn][move::piece(move)][move::sq2(move)]) };
		*entry += depth * depth;

		if (*entry > (1ULL << 63))
			for (auto &i : history.list) for (auto &j : i) for (auto &h : j) h >>= 2;
	}

	void heuristics(const board &pos, uint32 move, uint32 &counter, int depth, int curr_depth)
	{
		// updating history & killer & counter move if a quiet move failes high

		if (move::is_quiet(move))
		{
			hist_table(pos, move, depth);
			kill_table(move, curr_depth);
			counter = move;
		}
	}

	void evolving_pv(int curr_depth, uint32 move)
	{
		// saving the partial pv if a new best move has been found

		evolving.pv[curr_depth - 1][0] = move;
		for (auto d{ 0 }; evolving.pv[curr_depth][d] != NO_MOVE; ++d)
			evolving.pv[curr_depth - 1][d + 1] = evolving.pv[curr_depth][d];
	}

	void main_pv(uint32 move, uint32 pv_move[])
	{
		// updating the whole pv if a new best root move has been found

		pv_move[0] = move;
		for (auto d{ 0 }; evolving.pv[0][d] != NO_MOVE; ++d)
			pv_move[d + 1] = evolving.pv[0][d];
	}
}

namespace draw
{
	// detecting draws independent of the static evaluation

	bool repetition(const board &pos, int curr_depth)
	{
		// marking every one-fold-repetition as a draw

		auto size{ engine::move_offset + curr_depth };

		assert(hash[size] == pos.key);
		assert(engine::move_offset + curr_depth <= pos.move_cnt);

		for (auto i{ 4 }; i <= pos.half_move_cnt && i <= size; i += 2)
		{
			if (hash[size - i] == hash[size])
				return true;
		}
		return false;
	}

	bool verify(const board &pos, int curr_depth)
	{
		// detecting draw-by-3-fold-repetition and draw-by-50-move-rule

		hash[engine::move_offset + curr_depth] = pos.key;

		if (pos.half_move_cnt >= 4
			&& (repetition(pos, curr_depth) || pos.half_move_cnt >= 100))
			return true;
		else
			return false;
	}
}

void search::init_stack(search_stack *stack)
{
	for (auto d{ 0 }; d <= lim::depth; ++d)
	{
		stack[d] = { d, NO_MOVE, {NO_MOVE, NO_SCORE, 0}, {0ULL, 0}, false };
	}
}

void search::init_multipv(std::vector<variation> &multipv)
{
	for (auto &pv : multipv)
	{
		for (auto &m : pv.move) m = NO_MOVE;
		pv.mindepth = 0;
		pv.maxdepth = 0;
		pv.seldepth = 0;
		pv.score = MIN_SCORE;
	}
}

uint32 search::id_frame(board &pos, uint64 &movetime, uint32 &ponder)
{
	// iterative deepening framework

	assert(engine::limit.depth >= 1);
	assert(engine::limit.depth <= lim::depth);
	assert(engine::multipv >= 1);

	// starting the clock & getting search parameters ready

	expire::start_clock(movetime);
	search_stack stack[lim::depth + 1];
	std::vector<variation> pv(lim::multipv);

	int score{ NO_SCORE };
	uint32 best_move{ };

	// initialising search & generating and sorting moves

	init_stack(stack);
	init_multipv(pv);
	init::search();

	movepick_root pick(pos);

	// iterative deepening & looping through multi-principal variations

	for (auto depth{ 1 }; depth <= engine::limit.depth && !engine::stop; ++depth)
	{
		for (auto idx{ 0 }; idx < engine::multipv && idx <= pick.list.cnt.moves && !engine::stop; ++idx)
		{
			// rearranging root moves & storing the PV from the last iteration

			pick.rearrange_moves(pv[idx].move[0], idx > 0 ? pv[idx - 1].move[0] : NO_MOVE);

			// starting alphabeta search through an aspiration window

			try { score = aspiration(pos, stack, pick, pv[idx], depth, idx + 1); }
			catch (int &exception)
			{
				if (exception == 1) pos.revert(pick.list.pos);
				else assert(false);
			}

			chrono.split();
			assert(score == MIN_SCORE || nodes.cnt != 0);
			//assert(pv[idx].score < MATE_SCORE || score >= pv[idx].score);

			// defining depth derivates

			pv[idx].mindepth = depth - engine::stop;
			pv[idx].seldepth = pv::get_seldepth(pv[idx].move, pv[idx].seldepth, depth);

			// sanity-checking the PV only after a successfull search

			if (score != MIN_SCORE)
			{
				pv[idx].score = score;
				pv::prune(pv[idx], pos);
				pv::store(pv[idx], pos);

				//assert(!score::mate(pv[idx].score) || abs(pv[idx].score) == MAX_SCORE - pv[idx].maxdepth);
			}
		}

		// outputting search information & selecting best moves
		
		output::info(pv);
		best_move = pv[0].move[0];
		ponder    = pv[0].move[1];

		if (expire::abort(depth, pv[0], pick))
			break;
	}
	nodes.total += nodes.cnt;
	searchtime += chrono.elapsed;
	return best_move;
}

int search::aspiration(board &pos, search_stack *stack, movepick_root &pick, variation &pv, int depth, int multipv)
{
	// aspiration window, currently deactivated

	int alpha{ MIN_SCORE }, beta{ MAX_SCORE };
	int alpha_margin{ 10 }, beta_margin{ 10 };
	int score{ NO_SCORE };
	int bound{ EXACT };

	while (true)
	{
		if (depth >= /*4*/ lim::depth)
		{
			assert(pv.score != NO_SCORE);
			alpha = std::max(pv.score - alpha_margin, (int)MIN_SCORE);
			beta  = std::min(pv.score +  beta_margin, (int)MAX_SCORE);
		}
		assert(MIN_SCORE <= alpha && alpha < beta && beta <= MAX_SCORE);

		score = root_alphabeta(pos, stack, pick, pv, depth, alpha, beta, multipv);
		assert(score != NO_SCORE);

		if (score <= alpha)
		{
			bound = UPPER;
			alpha_margin *= 2;
		}
		else if (score >= beta)
		{
			bound = LOWER;
			beta_margin *= 2;
		}

		if (!score::mate(score) && (score <= alpha || score >= beta))
			output::bound_info(pv, depth, multipv, score, bound);
		else break;
	}
	return score;
}

int search::root_alphabeta(board &pos, search_stack *stack, movepick_root &pick, variation &pv, int depth, int alpha, int beta, int multipv)
{
	// starting the alphabeta search with the root nodes

	assert(depth >= 1 && depth <= lim::depth);
	assert(MIN_SCORE <= alpha && alpha < beta && beta <= MAX_SCORE);

	int score{ NO_SCORE };

	// looping through the movelist

	for (auto i{ 0 }, moves{ 0 }; i < pick.list.cnt.moves; ++i)
	{
		if (pick.sort.root[i].skip)
			continue;
		stack->move = pick.sort.root[i].move;

		assert_exp(pos.pseudolegal(stack->move));
		assert(pick.list.in_list(stack->move));

		output::currmove(depth, multipv, stack->move, moves + 1);
		pick.sort.root[i].nodes -= nodes.cnt;

		pos.new_move(stack->move);
		nodes.cnt += 1;
		assert_exp(position::legal(pos));

		// check extension & PVS

		int ext{ pick.sort.root[i].check };
		if (moves > 0)
		{
			score = -alphabeta(pos, stack + 1, depth - 1 + ext, -alpha - 1, -alpha);
			if (score > alpha)
				score = -alphabeta(pos, stack + 1, depth - 1 + ext, -beta, -alpha);
		}
		else
			score = -alphabeta(pos, stack + 1, depth - 1 + ext, -beta, -alpha);

		pick.sort.root[i].nodes += nodes.cnt;
		pos.revert(pick.list.pos);
		assert(score > MIN_SCORE && score < MAX_SCORE);
		assert(score != NO_SCORE);
		//assert(score >= alpha || (score <= alpha && alpha < MATE_SCORE));

		if (score > alpha)
		{
			if (score >= beta)
				return score;

			alpha = score;
			update::main_pv(stack->move, pv.move);
			pick.sort.root[i].nodes += nodes.cnt;
		}
		moves += 1;
	}
	return alpha;
}

int search::alphabeta(board &pos, search_stack *stack, int depth, int alpha, int beta)
{
	// main alphabeta search

	assert(MIN_SCORE <= alpha && alpha < beta && beta <= MAX_SCORE);
	assert(depth <= lim::depth);

	// detecting draws & dropping into quiescence search at leaf nodes

	if (draw::verify(pos, stack->depth))
		return engine::contempt[pos.xturn];
		
	if (depth <= 0 || stack->depth >= lim::depth)
		return qsearch(pos, stack, 0, alpha, beta);
		
	expire::check();

	// mate distance pruning

	auto a_bound{ alpha < MIN_SCORE + stack->depth ? MIN_SCORE + stack->depth : alpha };
	auto b_bound{ beta  > MAX_SCORE - stack->depth ? MAX_SCORE - stack->depth : beta  };
	if (b_bound <= a_bound)
		return a_bound;

	// transposition table lookup

	bool pv_node{ beta != alpha + 1};
	if (trans::probe(pos, stack->tt.move, stack->tt.score, stack->tt.bound, depth, stack->depth))
	{
		if (!pv_node)
			stack->tt.move = NO_MOVE;

		if (stack->tt.score <= alpha || stack->tt.score >= beta)
		{
			if (pv_node)
				stack->tt.move = NO_MOVE;

			else if (stack->tt.bound == EXACT
				||  (stack->tt.bound == LOWER && stack->tt.score >= beta)
				||  (stack->tt.bound == UPPER && stack->tt.score <= alpha))
				return stack->tt.score;
		}
	}

	// initialising pruning & evaluating the current position

	auto in_check{ pos.check() };
	auto crucial_node{ pv_node || in_check };
	auto skip_pruning{ crucial_node || stack->no_pruning };
	auto score{ crucial_node ? NO_SCORE : eval::static_eval(pos) };

	if (score::refineable(score, stack->tt.score, stack->tt.bound))
		score = stack->tt.score;

	// static null move pruning

	if (depth <= 3
		&& !score::mate(beta)
		&& !skip_pruning
		&& score - depth * 50 >= beta)
	{
		return beta;
	}

	// razoring

	if (depth <= 3
		&& !skip_pruning
		&& score + depth * 50 + 100 <= alpha)
	{
		auto raz_alpha{ alpha - depth * 50 - 100 };
		auto new_score{ qsearch(pos, stack, 0, raz_alpha, raz_alpha + 1) };
		if (new_score <= raz_alpha)
			return alpha;
	}

	// null move pruning

	if (depth >= 2
		&& !skip_pruning
		&& !pos.lone_king()
		&& score >= beta)
	{
		auto R{ 3 + std::min(3, (score - beta) / 128) };

		null::make_move(pos, stack);
		nodes.cnt += 1;
		auto null_score{ -alphabeta(pos, stack + 1, depth - 1 - R, -beta, 1 - beta) };
		null::revert_move(pos, stack);

		if (null_score >= beta)
			return beta;
	}

	// internal iterative deepening

	if (pv_node
		&& stack->tt.move == NO_MOVE
		&& !stack->no_pruning
		&& depth >= 3)
	{
		stack->no_pruning = true;
		alphabeta(pos, stack, depth - 2, alpha, beta);
		stack->no_pruning = false;

		trans::probe(pos, stack->tt.move, stack->tt.score, stack->tt.bound, depth, stack->depth);
	}

	// initialising move loop

	auto is_futile{ !crucial_node && depth <= 6 };
	auto fut_score{ score + 50 + 100 * depth };
	auto best_score{ -MAX_SCORE };

	auto &counter{ counter_move[move::piece((stack - 1)->move)][move::sq2((stack - 1)->move)] };
	auto prev_alpha{ alpha };

	// generating and sorting moves while looping through them

	movepick pick(pos, stack->tt.move, counter, history, killer, stack->depth);
	for (stack->move = pick.next(); stack->move; stack->move = pick.next())
	{
		// doing the move

		assert_exp(pos.pseudolegal(stack->move));
		pos.new_move(stack->move);
		if (!position::legal(pos))
		{
			pos.revert(pick.list.pos);
			pick.attempts -= 1;
			continue;
		}
		nodes.cnt += 1;

		// initialising move

		auto gives_check{ pos.check() };
		auto is_dangerous{ gives_check || !move::is_quiet(stack->move)
			|| move::is_pawn_advance(stack->move) || move::is_castling(stack->move)
			|| move::is_killer(stack->move, counter, stack->depth) };

		// futility pruning

		if (is_futile && !is_dangerous && pick.attempts > 0 && fut_score <= alpha && !score::mate(alpha))
		{
			pos.revert(pick.list.pos); continue;
		}

		// late move pruning

		if (!skip_pruning && !is_dangerous && depth <= 3 && pick.attempts >= depth * 4)
		{
			pos.revert(pick.list.pos); continue;
		}

		// extensions & PVS

		auto ext{ move::extend(stack->move, gives_check, pv_node, depth, pick.list.pos) };

		if (pv_node && pick.attempts > 0)
		{
			score = -alphabeta(pos, stack + 1, depth - 1 + ext, -alpha - 1, -alpha);
			if (score > alpha)
				score = -alphabeta(pos, stack + 1, depth - 1 + ext, -beta, -alpha);
		}
		else
			score = -alphabeta(pos, stack + 1, depth - 1 + ext, -beta, -alpha);

		pos.revert(pick.list.pos);
		assert(score != NO_SCORE);

		// checking for a new best move

		if (score > best_score)
		{
			best_score = score;
			if (score > alpha)
			{
				// checking for a beta cutoff

				if (score >= beta)
				{
					trans::store(pos, NO_MOVE, score, LOWER, depth, stack->depth);
					update::heuristics(pos, stack->move, counter, depth, stack->depth);

					nodes.fail_high += 1;
					nodes.fail_high_1st += (pick.attempts == 0);
					return score;
				}
				alpha = score;
				update::evolving_pv(stack->depth, stack->move);
			}
		}
	}

	// storing results & detecting checkmate/stalemate

	if (alpha == prev_alpha)
	{
		if (pick.attempts == -1)
			return in_check ? stack->depth - MAX_SCORE : engine::contempt[pos.xturn];

		trans::store(pos, NO_MOVE, best_score, UPPER, depth, stack->depth);
	}
	else
	{
		assert(alpha > prev_alpha);
		trans::store(pos, evolving.pv[stack->depth - 1][0], best_score, EXACT, depth, stack->depth);
	}
	
	assert(best_score > MIN_SCORE && best_score < MAX_SCORE);
	return best_score;
}

int search::qsearch(board &pos, search_stack *stack, int depth, int alpha, int beta)
{
	assert(MIN_SCORE <= alpha && alpha < beta && beta <= MAX_SCORE);
	assert(depth <= 0);
	assert_exp(depth != 0 || !pos.check());

	// detecting draws

	if (draw::verify(pos, stack->depth))
		return engine::contempt[pos.xturn];
		
	expire::check();
	
	// transposition table lookup

	if (depth == 0 && trans::probe(pos, stack->tt.move, stack->tt.score, stack->tt.bound, depth, stack->depth))
	{
		if (stack->tt.bound == EXACT
			|| (stack->tt.bound == LOWER && stack->tt.score >= beta)
			|| (stack->tt.bound == UPPER && stack->tt.score <= alpha))
			return stack->tt.score;
	}

	// standing pat & considering checks

	auto stand_pat{ eval::static_eval(pos) };
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
	for (stack->move = pick.next(); stack->move; stack->move = pick.next())
	{
		assert(depth >= -1 || !move::is_quiet(stack->move));
		assert_exp(pos.pseudolegal(stack->move));
		auto skip_pruning{ in_check || move::is_quiet(stack->move) };

		// depth limit pruning

		if (!skip_pruning
			&& depth <= -6
			&& !pos.recapture(stack->move))
			continue;

		// delta pruning

		if (!skip_pruning
			&& !pos.lone_king()
			&& !move::is_promo(stack->move)
			&& stand_pat + see::value[move::victim(stack->move)] + 100 < alpha)
			continue;

		// SEE pruning

		if (!skip_pruning
			&& !move::is_promo(stack->move)
			&& see::value[move::piece(stack->move)] > see::value[move::victim(stack->move)]
			&& see::eval(pos, stack->move) < 0)
			continue;

		pos.new_move(stack->move);
		assert_exp(position::legal(pos));
		nodes.cnt += 1;
		nodes.qs += 1;

		auto score{ -qsearch(pos, stack + 1, depth - 1, -beta, -alpha) };
		pos.revert(pick.list.pos);
		assert(score != NO_SCORE);

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

	if (in_check && pick.attempts == -1)
		return stack->depth - MAX_SCORE;

	assert(best_score > MIN_SCORE && best_score < MAX_SCORE);
	return best_score;
}