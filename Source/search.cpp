/*
  Monolith 0.1  Copyright (C) 2017 Jonas Mayr

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


#include "files.h"
#include "evaluation.h"
#include "hash.h"
#include "convert.h"
#include "movegen.h"
#include "engine.h"
#include "game.h"
#include "search.h"

namespace
{
	//// analysis variables
	uint64 all_nodes;
	double last_nodes;
	double ebf;
	int ebf_count;

	//// search variables
	uint16 pv_evol[lim::depth][lim::depth];
	const auto last{ lim::depth - 1 };

	uint64 hashlist[lim::period];
	bool no_pruning[lim::depth]{ false };

	int max_time;
	timer total_time;
	movegen root;

	//// search helping functions
	bool time_is_up()
	{
		return total_time.elapsed() >= max_time;
	}
	bool is_check(pos &board)
	{
		movegen gen;
		return gen.check(board, board.turn, board.pieces[KINGS] & board.side[board.turn]) == 0ULL;
	}
	bool is_promo(uint16 move)
	{
		return to_flag(move) < 12;
	}
}

void analysis::reset()
{
	all_nodes = 0;
	last_nodes = 0;
	ebf = 0;
	ebf_count = 0;
}

#ifdef DEBUG

void analysis::summary(timer &time)
{
	auto analysis_time{ time.elapsed() };
	ebf /= ebf_count;

	log::cout.precision(3);
	log::cout << std::fixed << endl
		<< "=========================\n"
		<< "time : " << analysis_time << " ms\n"
		<< "nodes: " << all_nodes << "\n"
		<< "nps  : " << all_nodes / analysis_time / 1000.0 << "M\n"
		<< "ebf  : " << ebf << endl << endl;
}
void analysis::root_perft(pos &board, int depth_min, int depth_max)
{
	assert(depth_min >= 1);
	assert(depth_max >= 1);
	ebf_count = 1;

	for (int depth{ depth_min }; depth <= depth_max; ++depth)
	{
		timer perft_time;
		log::cout << "perft " << depth << ": ";

		uint64 nodes{ perft(board, depth) };
		all_nodes += nodes;

		log::cout.precision(3);
		log::cout << nodes << " time " << perft_time.elapsed()
			<< " nps " << std::fixed << nodes / (perft_time.elapsed() + 1) / 1000.0 << "M" << endl;
	}
}
uint64 analysis::perft(pos &board, int depth)
{
	uint64 nodes{ 0 };

	/*
	if (depth == 1)
	{
		movegen gen(board, ALL);
		return gen.move_cnt;
	}*/
	if (depth == 0) return 1;

	movegen gen(board, ALL);

	pos save(board);

	for (int i{ 0 }; i < gen.move_cnt; ++i)
	{
		board.new_move(gen.movelist[i]);

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
	
	total_time.start();
	max_time = chrono.get_movetime(board.turn);
	
	uint16 pv[lim::depth]{ 0 };
	uint16 best_move{ 0 };
	string ply_str, score_str;

	for (int i{ abs(game::moves - 2) }; i <= game::moves; ++i)
		hashlist[i] = game::hashlist[i];

	for (auto &i : pv_evol)
	{
		for (auto &p : i) p = 0;
	}

	root.gen_moves(board, ALL);
	if (root.move_cnt == 1)
	{
		return root.movelist[0];
	}

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

			//// effective branching factor
			all_nodes += board.nodes;
			if (last_nodes != 0)
			{
				ebf += board.nodes / last_nodes;
				ebf_count += 1;
			}
			last_nodes = static_cast<double>(board.nodes);

			//// nodes per second
			assert(board.nodes != 0);
			auto nps{ static_cast<int>(board.nodes * 1000 / (interim + 1)) };
			string nodes_str{ std::to_string(board.nodes) };

			//// depth & seldepth
			auto seldepth{ ply };
			for (; seldepth < lim::depth; ++seldepth)
			{
				if (pv[seldepth] == 0)
					break;
			}
			ply_str = std::to_string(ply);
			if (engine::stop) ply_str = std::to_string(ply - 1);

			//// cutting redundant pv-entries after mate
			for (int i{ score_e::MATE - abs(score) }; i < seldepth; pv[i++] = 0);

			//// cutting redundant pv-entries after draw
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

			//// score
			if (abs(score) >= score_e::MAX)
				score_str = "mate " + std::to_string((score_e::MATE - abs(score) + 1) / 2);
			else
				score_str = "cp " + std::to_string(score);
			
			log::cout << "info"
				<< " depth " << ply_str
				<< " seldepth " << std::to_string(seldepth)
				<< " score " << score_str
				<< " nodes " << nodes_str
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

			//// allowing 5 extra plies when drawing or getting mated
			if (ply >= 5 && pv[ply - 5] == 0)
				break;
		}
		else break;
	}

	return best_move;
}
void search::root_alphabeta(pos &board, uint16 pv[], int &best_score, int ply)
{
	//// principal variation from the last iteration
	if (pv[0])
	{
		assert(root.in_list(pv[0]));

		std::swap(root.movelist[0], root.movelist[1]);
		std::swap(root.movelist[0], *root.find(pv[0]));

		for (int i{ 0 }; pv[i] != 0; ++i)
		{
			pv_evol[last][i] = pv[i];
		}
	}

	pos saved(board);
	int alpha{ -score_e::MATE };
	int beta{ score_e::MATE };
	int score;

	for (int nr{ 0 }; nr < root.move_cnt && !engine::stop; ++nr)
	{
		assert(root.movelist[nr] != 0);
		assert(beta > alpha);

		board.new_move(root.movelist[nr]);

		score = -alphabeta(board, ply - 1, 1, -alpha, -beta);
		board = saved;

		if (score > alpha && !engine::stop)
		{
			alpha = score;
			pv[0] = root.movelist[nr];
			for (int i{ 0 }; pv_evol[0][i] != 0; ++i)
			{
				pv[i + 1] = pv_evol[0][i];
			}

			hashing::tt_save(board, pv[0], score, ply, LOWER);
		}
	}

	//// engine::stop
	if (alpha == -score_e::MATE)
	{
		pv[0] = 0;
		return;
	}

	best_score = alpha;
	hashing::tt_save(board, pv[0], best_score, ply, EXACT);
}
int search::alphabeta(pos &board, int ply, int depth, int beta, int alpha)
{
	assert(beta > alpha);

	//// draw
	hashlist[board.moves - 1] = board.key;
	if (draw::verify(board, hashlist))
		return score_e::DRAW;
	
	//// drop into quiescence search
	if (ply == 0 || depth >= lim::depth)
		return qsearch(board, alpha, beta);

	//// time check
	if (ply > 1 && time_is_up())
	{
		engine::stop = true;
		return 0;
	}

	//// mate distance pruning
	if (score_e::MATE - depth < beta)
	{
		beta = score_e::MATE - depth;
		if (score_e::MATE - depth <= alpha)
			return alpha;
	}

	//// initialise pruning
	bool skip_pruning{ is_check(board) || pv_evol[last][depth] || no_pruning[depth] };
	int score{ score_e::NDEF };

	//// transposition table lookup
	uint16 tt_move{ 0 };
	uint8 tt_flag{ 0 };
	if (hashing::tt_probe(ply, alpha, beta, board, score, tt_move, tt_flag))
	{
		assert(score != score_e::NDEF);
		assert(tt_move != 0);
		assert(tt_flag != 0);

		if (score > score_e::MAX) score -= depth;
		if (score < -score_e::MAX) score += depth;

		if (score >= beta || score <= alpha)
		{
			if (tt_flag == UPPER && score >= beta) return score;
			if (tt_flag == LOWER && score <= alpha) return score;
			if (tt_flag == EXACT) return score;
		}
	}
	if (tt_flag != EXACT || score <= alpha || score >= beta)
		tt_move = 0;

	if (!skip_pruning)
	{
		//// static evaluation
		if (ply <= 3)
			score = eval::eval_board(board);

		//// beta pruning
		if (ply <= 3
			&& abs(beta) < score_e::MAX
			&& score - ply * 50 >= beta)
		{
			assert(score != score_e::NDEF);
			return score;
		}

		//// razoring
		if (ply <= 3
			&& score + ply * 50 + 100 <= alpha)
		{
			int raz_alpha{ alpha - ply * 50 - 100 };
			int new_s{ qsearch(board, raz_alpha, raz_alpha + 1) };

			if (new_s <= raz_alpha)
				return new_s;
		}

		//// null move pruning
		if (ply >= 2
			&& !board.lone_king()
			&& ((score != score_e::NDEF && score >= beta)
				|| (score == score_e::NDEF && eval::eval_board(board) >= beta)))
		{
			int R{ ply > 6 ? 3 : 2 };
			uint64 ep_copy{ 0 };

			board.null_move(ep_copy);

			no_pruning[depth + 1] = true;
			score = -alphabeta(board, ply - R, depth + 1, 1 - beta, -beta);
			no_pruning[depth + 1] = false;

			if (engine::stop) return 0;

			board.undo_null_move(ep_copy);

			if (score >= beta) return score;
		}
	}
	
	//// generating all moves
	movegen gen(board, ALL);

	//// checkmate & stalemate
	if (!gen.move_cnt)
	{
		if (is_check(board))
			alpha = depth - score_e::MATE;
		else
			alpha = score_e::DRAW;

		return alpha;
	}

	//// principal variation from last iteration
	if (pv_evol[last][depth])
	{
		uint16 *pos_list{ gen.find(pv_evol[last][depth]) };

		if (pos_list != gen.movelist + gen.move_cnt)
			std::swap(gen.movelist[0], *pos_list);
		else
			for (int i{ depth + 1 }; pv_evol[last][i] != 0; pv_evol[last][i++] = 0);

		pv_evol[last][depth] = 0;
	}

	//// move from the transposition table
	else if (tt_move != 0)
	{
		uint16 *pos_list{ gen.find(tt_move) };

		if (pos_list != gen.movelist + gen.move_cnt)
			std::swap(gen.movelist[0], *pos_list);
	}

	//// internal iterative deepening
	/*else if (ply >= 3 && !no_pruning[depth])
	{
		no_pruning[depth] = true;
		score = alphabeta(board, ply - 2, depth, beta, alpha);
		no_pruning[depth] = false;

		if (engine::stop) return 0;

		if (score > alpha)
		{
			uint16 *pos_list{ gen.find(pv_evol[depth - 1][0]) };

			if (pos_list != gen.movelist + gen.move_cnt)
				std::swap(gen.movelist[0], *pos_list);
		}
	}*/

	//// going through the movelist
	pos save(board);
	for (int nr{ 0 }; nr < gen.move_cnt; ++nr)
	{
		assert(gen.movelist[nr] != 0);

		board.new_move(gen.movelist[nr]);

		//// check extension
		int ext{ 1 };
		if (is_check(board) && ply <= 4)
			ext = 0;

		score = -alphabeta(board, ply - ext, depth + 1, -alpha, -beta);

		if (engine::stop) return 0;
		board = save;

		if (score >= beta)
		{
			hashing::tt_save(board, gen.movelist[nr], score, ply, UPPER);
			return score;
		}
		if (score > alpha)
		{
			alpha = score;
			hashing::tt_save(board, gen.movelist[nr], score, ply, EXACT);
			
			//// updating principal variation
			pv_evol[depth - 1][0] = gen.movelist[nr];
			for (int i{ 0 }; pv_evol[depth][i] != 0; ++i)
			{
				pv_evol[depth - 1][i + 1] = pv_evol[depth][i];
			}
		}
		else
		{
			hashing::tt_save(board, gen.movelist[nr], score, ply, LOWER);
		}
	}

	return alpha;
}
int search::qsearch(pos &board, int alpha, int beta)
{
	if (draw::by_material(board))
		return score_e::DRAW;

	movegen gen(board, CAPTURES);

	//// standing pat
	int score{ eval::eval_board(board) };
	if (!gen.move_cnt) return score;

	if (score >= beta) return score;
	if (score > alpha) alpha = score;

	pos save(board);
	for (int nr{ 0 }; nr < gen.move_cnt; ++nr)
	{
		//// delta pruning
		int piece{ board.piece_sq[to_sq2(gen.movelist[nr])] };
		if (!board.lone_king()
			&& !is_promo(gen.movelist[nr])
			&& score + value[piece] + 200 < alpha)
			continue;

		board.new_move(gen.movelist[nr]);

		score = -qsearch(board, -beta, -alpha);
		board = save;

		if (score >= beta) return beta;
		if (score > alpha) alpha = score;
	}
	return alpha;
}