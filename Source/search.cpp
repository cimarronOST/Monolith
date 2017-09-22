/*
  Monolith 0.3  Copyright (C) 2017 Jonas Mayr

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


#include "attack.h"
#include "see.h"
#include "bitboard.h"
#include "logfile.h"
#include "evaluation.h"
#include "hash.h"
#include "engine.h"
#include "search.h"

namespace
{
	// effective branching factor

	struct ebf_result
	{
		uint64 all_n;
		uint64 final_n;
		uint64 prev_n;
		double factor;
		int cnt;
	} ebf;

	// managing time

	struct time_management
	{
		int call_cnt{ 0 };
		uint64 max;
		chronometer manage;
	} mean_time;

	bool is_legal(pos &board)
	{
		return attack::check(board, board.not_turn, board.pieces[KINGS] & board.side[board.not_turn]) != 0ULL;
	}
}

// analysis functions for debugging

void analysis::reset()
{
	ebf.prev_n = 0;
	ebf.all_n = 0;
	ebf.final_n = 0;
	ebf.factor = 0;
	ebf.cnt = 0;
}

void analysis::summary(chronometer &time)
{
	auto interim{ time.elapsed() };
	ebf.factor /= (ebf.cnt != 0 ? ebf.cnt : 1);

	log::cout.precision(3);
	log::cout << std::fixed
		<< "=======================================\n\n"
		<< "time  : " << interim << " ms\n"
		<< "nodes : " << ebf.final_n << "\n"
		<< "nps   : " << ebf.final_n / interim / 1000.0 << "M\n"
		<< "ebf   : " << ebf.factor << "\n"
		<< std::endl;
}

void analysis::root_perft(pos &board, int depth, const GEN_MODE mode)
{
	assert(depth >= 1 && depth <= lim::depth);
	assert(mode == LEGAL || mode == PSEUDO);

	mean_time.manage.start();
	ebf.final_n += ebf.all_n;
	ebf.all_n = 0;

	for (int d{ 1 }; d <= depth; ++d)
	{
		log::cout << "perft " << d << ": ";

		uint64 nodes{ perft(board, d, mode) };
		auto interim{ mean_time.manage.elapsed() };

		ebf.all_n += nodes;

		log::cout.precision(3);
		log::cout << nodes
			<< " time " << interim
			<< " nps " << std::fixed << ebf.all_n / (interim + 1) / 1000.0 << "M" << std::endl;
	}
}

uint64 analysis::perft(pos &board, int depth, const GEN_MODE mode)
{
	uint64 nodes{ 0 };

	if (depth == 0) return 1;

	movegen list(board, mode);
	list.gen_tactical(), list.gen_quiet();

	pos saved(board);

	for (int i{ 0 }; i < list.cnt.moves; ++i)
	{
		assert(list.is_pseudolegal(list.movelist[i]));

		board.new_move(list.movelist[i]);

		if (mode == LEGAL) assert(is_legal(board));
		if (mode == PSEUDO && !is_legal(board))
		{
			board = saved;
			continue;
		}

		nodes += perft(board, depth - 1, mode);
		board = saved;
	}

	return nodes;
}

// actual search

namespace
{
	uint32 pv_evol[lim::depth][lim::depth];

	uint64 hashlist[lim::period];
	bool no_pruning[lim::depth]{ false };

	int contempt[]{ 0, 0 };

	// move ordering variables

	sort:: killer_list killer{ };
	sort::history_list history{ };
	const uint64 max_history{ 1ULL << 63 };

	uint64 root_nodes[lim::movegen]{ 0 };
	uint64 nodes{ 0 };

	// helping functions

	bool stop_thread()
	{
		if (++mean_time.call_cnt < 256)
			return false;

		mean_time.call_cnt = 0;
		if (engine::infinite) return false;

		if (nodes >= engine::nodes) return true;

		return mean_time.manage.elapsed() >= mean_time.max;
	}

	bool is_check(pos &board)
	{
		return attack::check(board, board.turn, board.pieces[KINGS] & board.side[board.turn]) == 0ULL;
	}

	bool is_mate(int score)
	{
		return abs(score) > MATE_SCORE;
	}

	bool is_promo(uint32 move)
	{
		static_assert(PROMO_QUEEN  == 15, "promo encoding");
		static_assert(PROMO_KNIGHT == 12, "promo encoding");

		return to_flag(move) >= 12;
	}

	bool is_quiet(uint32 move)
	{
		return to_victim(move) == NONE && !is_promo(move);
	}

	bool is_killer(uint32 move, int depth)
	{
		return move == killer.list[depth][0] || move == killer.list[depth][1];
	}

	bool is_pawn_to_7th(uint32 move)
	{
		return to_piece(move) == PAWNS && ((1ULL << to_sq2(move)) & (rank[R2] | rank[R7]));
	}

	void currmove(int ply, uint32 move, int move_nr)
	{
		auto interim{ mean_time.manage.elapsed() };

		if ((interim > 1000 && move_nr <= 3) || interim > 5000)
		{
			std::cout << "info depth " << ply
				<< " currmove " << algebraic(move)
				<< " currmovenumber " << move_nr
				<< " time " << interim
				<< " nps " << nodes * 1000 / interim 
				<< std::endl;
		}
	}

	// updates during search

	namespace update
	{
		void killer_moves(uint32 move, int depth)
		{
			if (move == killer.list[depth][0] || move == killer.list[depth][1])
				return;

			killer.list[depth][1] = killer.list[depth][0];
			killer.list[depth][0] = move;
		}

		void history_table(const pos &board, uint32 move, int ply)
		{
			assert(board.turn == to_turn(move));

			uint64 *entry{ &(history.list[board.turn][to_piece(move)][to_sq2(move)]) };
			*entry += ply * ply;

			if (*entry > max_history)
			{
				for (auto &i : history.list) for (auto &j : i) for (auto &h : j) h >>= 1;
			}
		}

		void heuristics(const pos &board, uint32 move, int ply, int depth)
		{
			if (is_quiet(move))
			{
				history_table(board, move, ply);
				killer_moves(move, depth);
			}
		}

		void temp_pv(int depth, uint32 move)
		{
			pv_evol[depth - 1][0] = move;
			for (int i{ 0 }; pv_evol[depth][i] != NO_MOVE; ++i)
			{
				pv_evol[depth - 1][i + 1] = pv_evol[depth][i];
			}
		}

		void main_pv(uint32 move, uint32 pv[], int real_nr)
		{
			pv[0] = move;
			for (int i{ 0 }; pv_evol[0][i] != NO_MOVE; ++i)
			{
				pv[i + 1] = pv_evol[0][i];
			}

			root_nodes[real_nr] += nodes;
		}
	}

	// detecting draws

	namespace draw
	{
		const uint64 all_sq[]{ 0xaa55aa55aa55aa55, 0x55aa55aa55aa55aa };

		bool lone_bishops(const pos &board)
		{
			return (board.pieces[BISHOPS] | board.pieces[KINGS]) == board.side[BOTH];
		}

		bool lone_knights(const pos &board)
		{
			return (board.pieces[KNIGHTS] | board.pieces[KINGS]) == board.side[BOTH];
		}

		bool by_repetition(const pos &board, int depth)
		{
			int size{ engine::move_cnt + depth - 1 };
			for (int i{ 4 }; i <= board.half_move_cnt && i <= size; i += 2)
			{
				if (hashlist[size - i] == hashlist[size])
					return true;
			}

			return false;
		}

		bool by_material(const pos &board)
		{
			if (lone_bishops(board) && (!(all_sq[WHITE] & board.pieces[BISHOPS]) || !(all_sq[BLACK] & board.pieces[BISHOPS])))
				return true;

			if (lone_knights(board) && bb::popcnt(board.pieces[KNIGHTS]) == 1)
				return true;

			return false;
		}

		bool verify(const pos &board, int depth)
		{
			hashlist[board.move_cnt - 1] = board.key;

			if (board.half_move_cnt >= 4 && by_repetition(board, depth))
				return true;

			else if (by_material(board))
				return true;

			else if (board.half_move_cnt == 100)
				return true;

			else
				return false;
		}
	}
}

void search::stop_ponder()
{
	mean_time.max += mean_time.manage.elapsed();
}

uint32 search::id_frame(pos &board, chronos &chrono, uint32 &ponder)
{
	assert(engine::depth >= 1);
	assert(engine::depth <= lim::depth);

	// initialising variables

	mean_time.call_cnt = 0;
	mean_time.max = chrono.get_movetime(board.turn);
	mean_time.manage.start();

	uint32 pv[lim::depth]{ 0 };
	uint32 best_move{ 0 };

	contempt[board.turn] = -engine::contempt;
	contempt[board.not_turn] = engine::contempt;

	for (int i{ 0 }; i < engine::move_cnt; ++i)
		hashlist[i] = engine::hashlist[i];

	// resetting

	ebf.prev_n = 0;
	ebf.all_n  = 0;

	for (auto &i : pv_evol)      for (auto &p : i) p = NO_MOVE;
	for (auto &i : killer.list)  for (auto &k : i) k = NO_MOVE;
	for (auto &i : history.list) for (auto &j : i) for (auto &h : j) h = 1000ULL;

	// generating & weighting root moves

	movepick pick(board, root_nodes);

	nodes = 0;
	if (pick.list.cnt.moves == 1)
		return pick.list.movelist[0];

	// iterative deepening loop

	for (int ply{ 1 }; ply <= engine::depth; ++ply)
	{
		// alphabeta search

		int score{ root_alphabeta(board, pick, pv, ply) };

		auto interim{ mean_time.manage.elapsed() };

		if (pv[0])
		{
			// calculating nodes per second

			assert(nodes != 0);
			auto nps{ nodes * 1000 / (interim + 1) };

			// defining depth derivates

			int mindepth{ engine::stop ? ply - 1 : ply };
			int maxdepth{ 0 };
			int seldepth{ ply };

			while (seldepth < lim::depth - 1 && pv[seldepth] != NO_MOVE)
				seldepth += 1;

			// cutting redundant PV-entries

			for (int d{ MAX_SCORE - abs(score) }; d <= seldepth - 1; pv[d++] = NO_MOVE);

			for (; maxdepth < lim::depth && pv[maxdepth] != NO_MOVE; maxdepth += 1)
			{
				if (!pick.list.is_pseudolegal(pv[maxdepth]))
				{
					for (int d{ maxdepth++ }; d < lim::depth && pv[d] != NO_MOVE; pv[d++] = NO_MOVE);
					break;
				}

				board.new_move(pv[maxdepth]);

				if (!is_legal(board))
				{
					for (int d{ maxdepth++ }; d < lim::depth && pv[d] != NO_MOVE; pv[d++] = NO_MOVE);
					break;
				}
			}
			board = pick.reverse;

			// assigning best move and ponder move

			best_move = pv[0];
			ponder    = pv[1];

			// storing the PV

			for (int d{ 0 }, neg{ 1 }; d < lim::depth && pv[d] != NO_MOVE; ++d, neg *= -1)
			{
				assert(pick.list.is_pseudolegal(pv[d]));
				assert(maxdepth - d > 0);

				tt::store(board, pv[d], score * neg, maxdepth - d + 1, d, EXACT);

				board.new_move(pv[d]);

				assert(is_legal(board));
			}
			board = pick.reverse;

			// updating effective branching factor

			if (ebf.prev_n != 0)
			{
				ebf.factor += static_cast<double>(nodes - ebf.all_n) / ebf.prev_n;
				ebf.cnt += 1;
			}
			ebf.prev_n = nodes - ebf.all_n;
			ebf.all_n = nodes;
			ebf.final_n += ebf.prev_n;

			// precising score

			std::string score_str{ "cp " + std::to_string(score) };
			if (is_mate(score))
				score_str = "mate " + std::to_string((MAX_SCORE - abs(score) + 1) / 2);
			
			// outputting search information

			log::cout << "info"
				<< " depth " << mindepth
				<< " seldepth " << seldepth
				<< " score " << score_str
				<< " time " << interim
				<< " nodes " << nodes
				<< " nps " << nps
				<< " hashfull " << tt::hashfull()
				<< " pv ";

			for (int d{ 0 }; d < ply && pv[d] != NO_MOVE; ++d)
				log::cout << algebraic(pv[d]) << " ";

			log::cout << std::endl;

			// terminating the search

			if (engine::stop)
				break;
			if (engine::infinite)
				continue;
			if (score > MATE_SCORE)
				break;
		}

		// time is up

		else break;

		// stopping search early

		if (ply > 8 && pv[ply - 8] == NO_MOVE)
			break;
		if (interim > mean_time.max / 2)
			break;
	}

	return best_move;
}

int search::root_alphabeta(pos &board, movepick &pick, uint32 pv[], int ply)
{
	assert(ply <= lim::depth);

	// updating root movelist weights

	if (pv[0] != NO_MOVE)
	{
		assert(pick.list.in_list(pv[0]));
		pick.rearrange_root(root_nodes, pick.list.find(pv[0]));

		pv[0] = NO_MOVE;
	}

	int beta{ MAX_SCORE };
	int alpha{ -beta };

	int score{ NO_SCORE };
	int move_nr{ 0 };

	// looping through the movelist

	for (auto move{ pick.next() }; move != nullptr; move_nr += 1, move = pick.next())
	{
		ASSERT(pick.list.is_pseudolegal(*move));
		assert(beta > alpha);
		assert(pick.list.in_list(*move));

		currmove(ply, *move, move_nr + 1);

		auto real_nr{ static_cast<uint32>(move - pick.list.movelist) };

		root_nodes[real_nr] -= nodes;
		board.new_move(*move);
		nodes += 1;

		ASSERT(is_legal(board));

		// PVS

		if (move_nr > 0)
		{
			score = -alphabeta(board, ply - 1, 1, -alpha, -alpha - 1);

			if (score > alpha)
				score = -alphabeta(board, ply - 1, 1, -alpha, -beta);
		}
		else
			score = -alphabeta(board, ply - 1, 1, -alpha, -beta);

		root_nodes[real_nr] += nodes;
		board = pick.reverse;

		if (engine::stop)
			break;

		if (score > alpha)
		{
			alpha = score;

			update::main_pv(*move, pv, real_nr);
			tt::store(board, pv[0], score, ply, 0, EXACT);
		}
	}

	return alpha;
}

int search::alphabeta(pos &board, int ply, int depth, int beta, int alpha)
{
	assert(beta > alpha);
	assert(ply >= 0 && ply < lim::depth);

	bool pv_node = beta != alpha + 1;

	// stopping search or dropping into quiescence search
	
	if (draw::verify(board, depth))
		return contempt[board.turn];

	if (ply == 0 || depth >= lim::depth)
		return qsearch(board, alpha, beta);

	if (stop_thread())
	{
		engine::stop = true;
		return NO_SCORE;
	}

	// mate distance pruning

	if (MAX_SCORE - depth < beta)
	{
		beta = MAX_SCORE - depth;
		if (beta <= alpha)
			return alpha;
	}

	// transposition table lookup

	int tt_score{ NO_SCORE };
	uint32 tt_move{ NO_MOVE };
	uint8 tt_flag{ 0 };

	if (tt::probe(board, tt_move, tt_score, ply, depth, tt_flag))
	{
		assert(tt_score != NO_SCORE);
		assert(tt_flag != 0);

		if (pv_node)
		{
			if (tt_score <= alpha || tt_score >= beta)
				tt_move = NO_MOVE;
		}
		else if (tt_score >= beta || tt_score <= alpha)
		{
			if (tt_flag == LOWER && tt_score >=  beta) return beta;
			if (tt_flag == UPPER && tt_score <= alpha) return alpha;
			if (tt_flag == EXACT) return tt_score;
		}
	}

	if (tt_flag != EXACT)
		tt_move = NO_MOVE;

	// initialising pruning & evaluating the position

	bool in_check{ is_check(board) };
	bool skip_pruning{ pv_node || in_check || no_pruning[depth] };

	int score{ pv_node || in_check ? NO_SCORE : eval::static_eval(board) };

	// static null move pruning

	if (ply <= 3
		&& !is_mate(beta)
		&& !skip_pruning
		&& score - ply * 50 >= beta)
	{
		assert(score != NO_SCORE);
		return beta;
	}

	// razoring

	if (ply <= 3
		&& !skip_pruning
		&& score + ply * 50 + 100 <= alpha)
	{
		auto raz_alpha{ alpha - ply * 50 - 100 };
		auto new_score{ qsearch(board, raz_alpha, raz_alpha + 1) };

		if (engine::stop)
			return NO_SCORE;

		if (new_score <= raz_alpha)
			return alpha;
	}

	// null move pruning

	if (ply >= 3
		&& !skip_pruning
		&& !board.lone_king()
		&& score >= beta)
	{
		int R{ 2 };
		uint64 ep_copy{ 0 };
		uint16 capt_copy{ 0 };

		board.null_move(ep_copy, capt_copy);
		nodes += 1;

		no_pruning[depth + 1] = true;
		score = -alphabeta(board, ply - R - 1, depth + 1, 1 - beta, -beta);
		no_pruning[depth + 1] = false;

		if (engine::stop)
			return NO_SCORE;

		board.undo_null_move(ep_copy, capt_copy);

		if (score >= beta)
		{
			tt::store(board, NO_MOVE, score, ply, depth, LOWER);
			return beta;
		}
	}

	// futility pruning condition

	int fut_eval{ NO_SCORE };
	if (ply <= 6 && !pv_node && !in_check)
	{
		assert(score != NO_SCORE);
		fut_eval = score + 50 + 100 * ply;
	}
	
	// generating and sorting moves while looping through them
		
	movepick pick(board, tt_move, history, killer, depth);

	int prev_alpha{ alpha };
	int move_nr{ 0 };

	for (auto move{ pick.next() }; move != nullptr; move_nr += 1, move = pick.next())
	{
		ASSERT(pick.list.is_pseudolegal(*move));

		board.new_move(*move);

		if (!is_legal(board))
		{
			assert(pick.list.mode == PSEUDO);
			board = pick.reverse;
			move_nr -= 1;
			continue;
		}

		nodes += 1;

		// extensions

		int ext{ 0 };
		bool new_check{ is_check(board) };

		if (new_check && (ply <= 4 || pv_node))
			ext = 1;
		else if (pv_node && pick.reverse.recapture(*move))
			ext = 1;
		else if (pv_node && is_pawn_to_7th(*move))
			ext = 1;

		// reverse futility pruning

		if (move_nr > 0 && fut_eval <= alpha && is_quiet(*move) && !is_mate(alpha) && !new_check && !is_killer(*move, depth))
		{
			board = pick.reverse;
			continue;
		}

		// PVS

		if (pv_node && move_nr > 0)
		{
			score = -alphabeta(board, ply - 1 + ext, depth + 1, -alpha, -alpha - 1);

			if (score > alpha)
				score = -alphabeta(board, ply - 1 + ext, depth + 1, -alpha, -beta);
		}
		else
			score = -alphabeta(board, ply - 1 + ext, depth + 1, -alpha, -beta);

		if (engine::stop)
			return NO_SCORE;

		board = pick.reverse;

		// failing high

		if (score >= beta)
		{
			tt::store(board, NO_MOVE, score, ply, depth, LOWER);
			update::heuristics(board, *move, ply, depth);

			return beta;
		}

		if (score > alpha)
		{
			alpha = score;

			tt::store(board, *move, score, ply, depth, EXACT);
			update::temp_pv(depth, *move);
		}
	}

	if (alpha == prev_alpha)
	{
		// detecting checkmate & stalemate

		if (move_nr == 0)
			return in_check ? depth - MAX_SCORE : contempt[board.turn];

		// storing nodes that failed low

		else
			tt::store(board, NO_MOVE, alpha, ply, depth, UPPER);
	}
		
	return alpha;
}

int search::qsearch(pos &board, int alpha, int beta)
{
	// verifying draws & timeouts

	if (draw::by_material(board))
		return contempt[board.turn];

	if (stop_thread())
	{
		engine::stop = true;
		return NO_SCORE;
	}

	// stand pat

	int score{ eval::static_eval(board) };

	if (score >= beta)
		return beta;

	if (score > alpha)
		alpha = score;

	// generating and sorting moves while looping through them

	movepick pick(board);

	for (auto move{ pick.next() }; move != nullptr; move = pick.next())
	{
		ASSERT(pick.list.is_pseudolegal(*move));

		// delta pruning

		if (!board.lone_king()
			&& !is_promo(*move)
			&& score + see::exact_value[to_victim(*move)] + 100 < alpha)
			continue;

		// SEE pruning

		if (!is_promo(*move)
			&& see::exact_value[to_piece(*move)] > see::exact_value[to_victim(*move)]
			&& see::eval(board, *move) < 0)
			continue;

		board.new_move(*move);
		nodes += 1;

		ASSERT(is_legal(board));

		score = -qsearch(board, -beta, -alpha);

		board = pick.reverse;

		if (engine::stop)
			return NO_SCORE;

		if (score >= beta)
			return beta;

		if (score > alpha)
			alpha = score;
	}

	return alpha;
}
