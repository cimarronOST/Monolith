/*
  Monolith 0.2  Copyright (C) 2017 Jonas Mayr

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


#include "bitboard.h"
#include "sort.h"
#include "files.h"
#include "evaluation.h"
#include "hash.h"
#include "convert.h"
#include "engine.h"
#include "game.h"
#include "search.h"

namespace
{
	// analysis variables

	struct results
	{
		uint64 all_nodes;
		double prev_nodes;
		double factor;
		int count;
	} ebf;

	// search variables

	uint16 pv_evol[lim::depth][lim::depth];
	const auto last{ lim::depth - 1 };

	uint64 hashlist[lim::period];
	bool no_pruning[lim::depth]{ false };

	// move ordering variables

	uint16 killer[lim::depth][2]{};
	int history[2][6][64]{};
	const int max_history{ 1 << 30 };

	struct time_management
	{
		int max;
		timer manage;
	} spare_time;

	movegen root;

	// search helping functions

	inline bool time_is_up()
	{
		return spare_time.manage.elapsed() >= spare_time.max;
	}
	inline bool is_check(pos &board)
	{
		movegen gen;
		return gen.check(board, board.turn, board.pieces[KINGS] & board.side[board.turn]) == 0ULL;
	}
	inline bool is_promo(uint16 move)
	{
		static_assert(PROMO_ROOK == 12, "promo encoding");
		return to_flag(move) >= PROMO_ROOK;
	}
	inline void update_killer(uint16 move, int depth)
	{
		if (move == killer[depth][0] || move == killer[depth][1])
			return;

		killer[depth][1] = killer[depth][0];
		killer[depth][0] = move;
	}
	inline void update_history(const pos &board, uint16 move, int piece, int ply)
	{
		auto *h{ &history[board.turn][piece][to_sq2(move)] };
		*h += ply * ply;

		if (*h > max_history)
		{
			for (auto &i : history) for (auto &j : i) for (auto &h : j) h /= 2;
		}
	}
	inline void update_heuristics(const pos &board, uint16 move, int ply, int depth)
	{
		auto flag{ to_flag(move) };

		// normal quiet move

		if (flag == NONE)
		{
			auto piece{ board.piece_sq[to_sq1(move)] };
			auto new_kill{ (move & 0xfff) | (piece << 12) };

			update_history(board, move, piece, ply);
			update_killer(new_kill, depth);
		}

		// castling

		else if (flag >= castl_e::SHORT_WHITE && flag <= castl_e::LONG_BLACK)
		{
			update_history(board, move, KINGS, ply);
			update_killer(move, depth);
		}
	}
}

void analysis::reset()
{
	ebf.all_nodes = 0;
	ebf.prev_nodes = 0;
	ebf.factor = 0;
	ebf.count = 0;
}

#ifdef DEBUG

void analysis::summary(timer &time)
{
	auto total_time{ time.elapsed() };
	ebf.factor /= ebf.count;

	log::cout.precision(3);
	log::cout << std::fixed
		<< "\n=========================\n"
		<< "time : " << total_time << " ms\n"
		<< "nodes: " << ebf.all_nodes << "\n"
		<< "nps  : " << ebf.all_nodes / total_time / 1000.0 << "M\n"
		<< "ebf  : " << ebf.factor << endl << endl;
}
void analysis::root_perft(pos &board, int depth_min, int depth_max)
{
	assert(depth_min >= 1 && depth_min <= lim::depth);
	assert(depth_max >= 1 && depth_max <= lim::depth);
	ebf.count = 1;

	for (int depth{ depth_min }; depth <= depth_max; ++depth)
	{
		timer perft_time;
		log::cout << "perft " << depth << ": ";

		uint64 nodes{ perft(board, depth) };
		ebf.all_nodes += nodes;

		log::cout.precision(3);
		log::cout << nodes << " time " << perft_time.elapsed()
			<< " nps " << std::fixed << nodes / (perft_time.elapsed() + 1) / 1000.0 << "M" << endl;
	}
}
uint64 analysis::perft(pos &board, int depth)
{
	uint64 nodes{ 0 };

	if (depth == 0) return 1;

	movegen gen(board, ALL);
	pos save(board);

	for (int i{ 0 }; i < gen.move_cnt; ++i)
	{
		board.new_move(gen.list[i]);

		nodes += perft(board, depth - 1);
		board = save;
	}

	return nodes;
}

#endif

uint16 search::id_frame(pos &board, chronos &chrono)
{
	assert(engine::depth >= 1);
	assert(engine::depth <= lim::depth);
	
	spare_time.manage.start();
	spare_time.max = chrono.get_movetime(board.turn);
	assert(spare_time.max > 0);
	
	// initialising variables

	uint16 pv[lim::depth]{ 0 };
	uint16 best_move{ 0 };

	for (int i{ abs(game::moves - 2) }; i <= game::moves; ++i)
		hashlist[i] = game::hashlist[i];

	// resetting

	for (auto &i : pv_evol) for (auto &p : i) p = 0;
	for (auto &i : killer)  for (auto &k : i) k = 0;
	for (auto &i : history) for (auto &j : i) for (auto &h : j) h = 1;

	// generating root moves

	root.gen_moves(board, ALL);
	if (root.move_cnt == 1)
		return root.list[0];

	// iterative deepening loop

	for (int ply{ 1 }; ply <= engine::depth; ++ply)
	{
		timer mean_time;
		board.nodes = 0;
		int score{ 0 };

		root_alphabeta(board, pv, score, ply);
		auto interim{ mean_time.elapsed() };

		if (pv[0])
		{
			best_move = pv[0];

			// effective branching factor

			ebf.all_nodes += board.nodes;
			if (ebf.prev_nodes != 0)
			{
				ebf.factor += board.nodes / ebf.prev_nodes;
				ebf.count += 1;
			}
			ebf.prev_nodes = static_cast<double>(board.nodes);

			// nodes per second

			assert(board.nodes != 0);
			auto nps{ static_cast<int>(board.nodes * 1000 / (interim + 1)) };

			// depth & seldepth

			auto seldepth{ ply };
			for (; seldepth < lim::depth; ++seldepth)
			{
				if (pv[seldepth] == 0)
					break;
			}
			auto real_depth{ ply };
			if (engine::stop)
				real_depth = ply - 1;

			// cutting redundant pv-entries

			for (int i{ score_e::MATE - abs(score) }; i < seldepth; pv[i++] = 0);

			if (score == score_e::DRAW)
			{
				pos saved(board);
				for (int i{ 0 }; pv[i] != 0; ++i)
				{
					movegen gen(board, ALL);
					if (!gen.in_list(pv[i]))
					{
						for (auto j{ i }; pv[j] != 0; pv[j++] = 0);
						break;
					}
					board.new_move(pv[i]);
				}
				board = saved;
			}

			// score

			string score_str{ "cp " + std::to_string(score) };
			if (abs(score) >= score_e::MAX)
				score_str = "mate " + std::to_string((score_e::MATE - abs(score) + 1) / 2);
			
			log::cout << "info"
				<< " depth " << real_depth
				<< " seldepth " << seldepth
				<< " score " << score_str
				<< " nodes " << board.nodes
				<< " nps " << nps
				<< " time " << interim
				<< " pv ";

			for (int i{ 0 }; i < ply && pv[i] != 0; ++i)
			{
				log::cout
					<< conv::to_str(to_sq1(pv[i]))
					<< conv::to_str(to_sq2(pv[i]))
					<< conv::to_promo(to_flag(pv[i]));
			}
			log::cout << endl;

			if (engine::stop || time_is_up() || score > score_e::MAX)
				break;

			// allowing 5 extra plies when drawing or getting mated

			if (ply >= 5 && pv[ply - 5] == 0)
				break;
		}
		else break;
	}

	return best_move;
}
void search::root_alphabeta(pos &board, uint16 pv[], int &best_score, int ply)
{
	uint16 *best_move{ nullptr };

	if (pv[0])
	{
		assert(root.in_list(pv[0]));

		best_move = root.find(pv[0]);

		for (int i{ 0 }; pv[i] != 0; ++i)
			pv_evol[last][i] = pv[i];
		pv[0] = 0;
	}

	sort list(board, root, best_move, history);
	pos saved(board);

	int beta{ score_e::MATE };
	int alpha{ -beta };

	for (auto move{ list.next(root) }; move && !engine::stop; move = list.next(root))
	{
		assert(beta > alpha);
		assert(root.in_list(move));

		board.new_move(move);

		int score{ -alphabeta(board, ply - 1, 1, -alpha, -beta) };
		board = saved;

		if (score > alpha && !engine::stop)
		{
			alpha = score;
			pv[0] = move;
			for (int i{ 0 }; pv_evol[0][i] != 0; ++i)
			{
				pv[i + 1] = pv_evol[0][i];
			}

			tt::store(board, pv[0], score, ply, 0, EXACT);
		}
	}

	best_score = alpha;
}
int search::alphabeta(pos &board, int ply, int depth, int beta, int alpha)
{
	assert(beta > alpha);

	// draw

	hashlist[board.moves - 1] = board.key;
	if (draw::verify(board, hashlist, depth))
		return score_e::DRAW;

	// drop into quiescence search

	if (ply == 0 || depth >= lim::depth)
		return qsearch(board, alpha, beta);

	// time check

	if (ply > 1 && time_is_up())
	{
		engine::stop = true;
		return 0;
	}

	// mate distance pruning

	if (score_e::MATE - depth < beta)
	{
		beta = score_e::MATE - depth;
		if (beta <= alpha)
			return alpha;
	}

	// transposition table lookup

	struct ttable
	{
		int score{ score_e::NDEF };
		uint16 move{ 0 };
		uint8 flag{ 0 };
	} t;

	if (tt::probe(board, t.move, t.score, ply, depth, t.flag))
	{
		assert(t.score != score_e::NDEF);
		assert(t.flag != 0);

		if (t.score >= beta || t.score <= alpha)
		{
			if (t.flag == UPPER && t.score >= beta) return t.score;
			if (t.flag == LOWER && t.score <= alpha) return t.score;
			if (t.flag == EXACT) return t.score;
		}
	}
	if (t.flag != EXACT || t.score <= alpha || t.score >= beta)
		t.move = 0;

	// initialise pruning

	bool skip_pruning{ pv_evol[last][depth] || no_pruning[depth] || is_check(board) };
	int score{ score_e::NDEF };

	if (ply <= 3 && !skip_pruning)
		score = eval::eval_board(board);

	// beta pruning

	if (ply <= 3
		&& abs(beta) < score_e::MAX
		&& !skip_pruning
		&& score - ply * 50 >= beta)
	{
		assert(score != score_e::NDEF);
		return score;
	}

	// razoring

	if (ply <= 3
		&& !skip_pruning
		&& score + ply * 50 + 100 <= alpha)
	{
		auto raz_alpha{ alpha - ply * 50 - 100 };
		auto new_s{ qsearch(board, raz_alpha, raz_alpha + 1) };

		if (new_s <= raz_alpha)
			return new_s;
	}

	// null move pruning

	if (ply >= 3
		&& !skip_pruning
		&& beta != score_e::DRAW ///
		&& !board.lone_king()
		&& eval::eval_board(board) >= beta)
	{
		int R{ 3 };
		uint64 ep_copy{ 0 };

		board.null_move(ep_copy);

		no_pruning[depth + 1] = true;
		score = -alphabeta(board, ply - R, depth + 1, 1 - beta, -beta);
		no_pruning[depth + 1] = false;

		if (engine::stop) return 0;

		board.undo_null_move(ep_copy);

		if (score >= beta)
		{
			tt::store(board, 0, score, ply, depth, UPPER);
			return score;
		}
	}

	movegen gen(board, ALL);
	uint16 *best_move{ nullptr };

	// checkmate & stalemate detection

	if (!gen.move_cnt)
	{
		if (is_check(board))
			alpha = depth - score_e::MATE;
		else
			alpha = score_e::DRAW;

		return alpha;
	}

	// principal variation from last iteration

	if (pv_evol[last][depth])
	{
		uint16 *pos_list{ gen.find(pv_evol[last][depth]) };

		if(pos_list != gen.list + gen.move_cnt)
			best_move = pos_list;

		pv_evol[last][depth] = 0;
	}

	// move from the transposition table

	else if (t.move != 0)
	{
		uint16 *pos_list{ gen.find(t.move) };

		if(pos_list != gen.list + gen.move_cnt)
			best_move = pos_list;
	}

	sort list(board, gen, best_move, history, killer, depth);
	pos save(board);
	auto old_alpha{ alpha };

	for (auto move{ list.next(gen) }; move; move = list.next(gen))
	{
		board.new_move(move);

		// check extension

		int ext{ 1 };
		if (is_check(board) && ply <= 4)
			ext = 0;

		score = -alphabeta(board, ply - ext, depth + 1, -alpha, -beta);

		if (engine::stop) return 0;
		board = save;

		if (score >= beta)
		{
			tt::store(board, 0, score, ply, depth, UPPER);
			update_heuristics(board, move, ply, depth);
			return score;
		}
		if (score > alpha)
		{
			alpha = score;
			tt::store(board, move, score, ply, depth, EXACT);
			
			// updating principal variation

			pv_evol[depth - 1][0] = move;
			for (int i{ 0 }; pv_evol[depth][i] != 0; ++i)
			{
				pv_evol[depth - 1][i + 1] = pv_evol[depth][i];
			}
		}
	}

	if(alpha == old_alpha)
		tt::store(board, 0, alpha, ply, depth, LOWER);

	return alpha;
}
int search::qsearch(pos &board, int alpha, int beta)
{
	if (draw::by_material(board))
		return score_e::DRAW;

	movegen gen(board, CAPTURES);

	// standing pat

	auto score{ eval::eval_board(board) }; 

	if (!gen.move_cnt) return score;
	if (score >= beta) return score;
	if (score > alpha) alpha = score;

	sort list(board, gen);
	pos save(board);

	for (auto move{ list.next(gen) }; move; move = list.next(gen))
	{
		// delta pruning

		if (!board.lone_king()
			&& !is_promo(move)
			&& score + eval::value[MG][board.piece_sq[to_sq2(move)]] + 100 < alpha)
			continue;

		board.new_move(move);

		score = -qsearch(board, -beta, -alpha);
		board = save;

		if (score >= beta) return beta;
		if (score > alpha) alpha = score;
	}
	return alpha;
}
