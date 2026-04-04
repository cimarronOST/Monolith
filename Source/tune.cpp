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


// credits for the Texel tuning approach go to Peter Österlund:
// https://www.chessprogramming.org/Texel%27s_Tuning_Method

#if !defined(NDEBUG)

#include <atomic>
#include <thread>
#include <cmath>
#include <format>
#include <vector>
#include <iostream>
#include <fstream>
#include <string>
#include <limits>
#include <ctime>

#include "main.h"
#include "types.h"
#include "thread.h"
#include "search.h"
#include "board.h"
#include "misc.h"
#include "eval.h"
#include "tune.h"

using namespace eval;

namespace tune
{
	constexpr int k_precision{ 7 };
	constexpr int weight_cnt{ 649 };
	struct tuning_pos { board pos; double result; };
}

namespace tune
{
	static void next(square& sq)
	{
		// skipping the left half of the piece-square-tables because it gets mirrored anyway

		sq += (sq - 3) % 8 ? 1 : 5;
	}

	static void cout(int sc)
	{
		// displaying the packed evaluation scores

		std::cout << "S(" << std::format("{:>4}", S_MG(sc)) << "," << std::format("{:>4}", S_EG(sc)) << "), ";
	}

	static void add(std::vector<int>& weights, int param)
	{
		// adding the packed evaluation scores to the weights vector

		weights.push_back(S_MG(param));
		weights.push_back(S_EG(param));
	}

	static int get(const std::vector<int>& weights, int& c)
	{
		// getting the evaluation scores from the weights vector packed

		int param{ S(weights[c], weights[c + 1]) }; c += 2;
		return param;
	}

	static bool open_file(std::ifstream& stream, const std::string& epd_file)
	{
		// opening the epd-file containing the tuning positions with results

		if (stream.is_open())
			stream.close();

		std::string path{ filesystem::path + epd_file };
		stream.open(path, std::ifstream::in);
		return stream.is_open();
	}

	static void load_pos(std::ifstream& stream, std::vector<tuning_pos>& pos)
	{
		// loading the positions from the epd-file
		// getting everything ready for the quiescence-search first

		board position{};
		thread_pool threads(1, position);
		threads.thread[0]->init();

		// reading the file

		std::string line{};
		while (std::getline(stream, line))
		{
			tuning_pos curr_pos{};
			auto fen_end{ line.find_last_of(' ', std::string::npos) };

			std::string result{ line.substr(fen_end + 1, std::string::npos) };
			if  (result == "1/2-1/2" || result == "[0.5]") curr_pos.result = 0.5;
			else if (result == "1-0" || result == "[1.0]") curr_pos.result = 1.0;
			else if (result == "0-1" || result == "[0.0]") curr_pos.result = 0.0;
			curr_pos.pos.parse_fen(line.substr(0, fen_end));

			// quiescence-search resolves non-quiet positions
			
			search::node::p_variation pv{};
			search::node nd{ &curr_pos.pos, &pv, false, false, false };
			nd.p_var->cnt = 0;
			nd.check = curr_pos.pos.check();
			auto stack{ threads.thread[0]->stack_front() };
			stack->cont_mv = &threads.thread[0]->hist.corr_cont[PAWN][H1];

			search::qsearch(*threads.thread[0], stack + 1, nd, 0, -MATE, MATE);
			for (depth dt{}; dt < std::min(pv.cnt, lim::dt); ++dt)
				curr_pos.pos.new_move(pv.mv[dt]);

			pos.push_back(curr_pos);
		}
	}

	static void load_files(const std::vector<std::string>& tuning_files, std::vector<tuning_pos>& pos)
	{
		// loading the tuning files

		std::ifstream stream{};
		for (auto& file : tuning_files)
		{
			if (open_file(stream, file))
				std::cout << "\nloading file " << file << " ..." << std::endl;
			else
				std::cout << "\nwarning: failed to open \"" << file << "\", path = " << filesystem::path << std::endl;

			// loading positions from the file

			load_pos(stream, pos);
			std::cout << "entries loaded  : " << pos.size() << std::endl;
			std::cout.precision(2);
			std::cout << "memory allocated: " << (sizeof(tuning_pos) * pos.size() >> 20) << " MB" << std::endl;
		}
	}

	static void load_weights(std::vector<int>& vec)
	{
		// loading all the evaluation weights into a vector

		vec.push_back(S_MG(piece_value[PAWN]));
		for (auto p{ KNIGHT }; p <= QUEEN; p = p + 1) add(vec, piece_value[p]);
		for (auto& w : scale_few_pawns) vec.push_back(w);
		vec.push_back(tempo);

		for (auto p{ KNIGHT }; p <= QUEEN; p = p + 1) vec.push_back(S_MG(threat_king_by_check[p]));
		for (auto p{ BISHOP }; p <= QUEEN; p = p + 1) vec.push_back(S_MG(threat_king_by_xray[p]));
		for (auto p{ KNIGHT }; p <= QUEEN; p = p + 1) vec.push_back(threat_king_weight[p]);
		for (int i{ 7 }; i < 60; ++i)                 vec.push_back(S_MG(threat_king_sum[i]));
		vec.push_back(S_MG(weak_king_sq));

		for (auto sq{ H2 }; sq <= A7; next(sq)) add(vec,   pawn_psq[BLACK][sq]);
		for (auto sq{ H1 }; sq <= A8; next(sq)) add(vec, knight_psq[BLACK][sq]);
		for (auto sq{ H1 }; sq <= A8; next(sq)) add(vec, bishop_psq[BLACK][sq]);
		for (auto sq{ H1 }; sq <= A8; next(sq)) add(vec,   rook_psq[BLACK][sq]);
		for (auto sq{ H1 }; sq <= A8; next(sq)) add(vec,  queen_psq[BLACK][sq]);
		for (auto sq{ H1 }; sq <= A8; next(sq)) add(vec,   king_psq[BLACK][sq]);

		add(vec, bishop_pair);
		add(vec, bishop_color_pawns);
		add(vec, bishop_trapped);
		add(vec, knight_outpost);
		for (auto& w : knight_distance_kings) vec.push_back(S_MG(w));
		add(vec, rook_on_7th);
		add(vec, rook_open_file);
		add(vec, threat_pawn);
		add(vec, threat_minor);
		add(vec, threat_rook);
		add(vec, threat_queen_by_minor);
		add(vec, threat_queen_by_rook);
		add(vec, threat_piece_by_pawn);
		add(vec, threat_piece_by_king);
		add(vec, isolated);
		add(vec, backward);
		add(vec, king_dist_passed_cl);
		add(vec, king_dist_passed_cl_x);
		add(vec, king_without_pawns);
		for (auto& w : complexity) vec.push_back(S_EG(w));

		for (int r{ RANK_2 }; r <= RANK_7; ++r)           add(vec, connect_rank[WHITE][r]);
		for (int r{ RANK_2 }; r <= RANK_7; ++r) vec.push_back(S_MG( passed_rank[WHITE][r]));
		for (int r{ RANK_2 }; r <= RANK_8; ++r) vec.push_back(S_MG( shield_rank[WHITE][r]));

		for (auto& w : knight_mobility) add(vec, w);
		for (auto& w : bishop_mobility) add(vec, w);
		for (auto& w : rook_mobility)   add(vec, w);
		for (auto& w : queen_mobility)  add(vec, w);
	}

	static void save_weights(const std::vector<int>& vec)
	{
		// copying the tuned evaluation weights back to the evaluation

		int c{};

		piece_value[PAWN] = S(vec[c++], S_EG(piece_value[PAWN]));
		for (auto p{ KNIGHT }; p <= QUEEN; p = p + 1) piece_value[p] = get(vec, c);
		for (auto& w : scale_few_pawns) w = vec[c++];
		tempo = vec[c++];

		for (auto p{ KNIGHT }; p <= QUEEN; p = p + 1) threat_king_by_check[p] = S(vec[c++], 0);
		for (auto p{ BISHOP }; p <= QUEEN; p = p + 1) threat_king_by_xray[p]  = S(vec[c++], 0);
		for (auto p{ KNIGHT }; p <= QUEEN; p = p + 1) threat_king_weight[p] = vec[c++];
		for (int i{ 7 }; i < 60; ++i)                 threat_king_sum[i] = S(vec[c++], 0);
		weak_king_sq = S(vec[c++], 0);

		for (auto sq{ H2 }; sq <= A7; next(sq)) {   pawn_psq[BLACK][sq] = get(vec, c);   pawn_psq[BLACK][type::sq_flip(sq)] =   pawn_psq[BLACK][sq]; }
		for (auto sq{ H1 }; sq <= A8; next(sq)) { knight_psq[BLACK][sq] = get(vec, c); knight_psq[BLACK][type::sq_flip(sq)] = knight_psq[BLACK][sq]; }
		for (auto sq{ H1 }; sq <= A8; next(sq)) { bishop_psq[BLACK][sq] = get(vec, c); bishop_psq[BLACK][type::sq_flip(sq)] = bishop_psq[BLACK][sq]; }
		for (auto sq{ H1 }; sq <= A8; next(sq)) {   rook_psq[BLACK][sq] = get(vec, c);   rook_psq[BLACK][type::sq_flip(sq)] =   rook_psq[BLACK][sq]; }
		for (auto sq{ H1 }; sq <= A8; next(sq)) {  queen_psq[BLACK][sq] = get(vec, c);  queen_psq[BLACK][type::sq_flip(sq)] =  queen_psq[BLACK][sq]; }
		for (auto sq{ H1 }; sq <= A8; next(sq)) {   king_psq[BLACK][sq] = get(vec, c);   king_psq[BLACK][type::sq_flip(sq)] =   king_psq[BLACK][sq]; }

		bishop_pair           = get(vec, c);
		bishop_color_pawns    = get(vec, c);
		bishop_trapped        = get(vec, c);
		knight_outpost        = get(vec, c);
		for (auto& w : knight_distance_kings) w = S(vec[c++], 0);
		rook_on_7th           = get(vec, c);
		rook_open_file        = get(vec, c);
		threat_pawn           = get(vec, c);
		threat_minor          = get(vec, c);
		threat_rook           = get(vec, c);
		threat_queen_by_minor = get(vec, c);
		threat_queen_by_rook  = get(vec, c);
		threat_piece_by_pawn  = get(vec, c);
		threat_piece_by_king  = get(vec, c);
		isolated              = get(vec, c);
		backward              = get(vec, c);
		king_dist_passed_cl   = get(vec, c);
		king_dist_passed_cl_x = get(vec, c);
		king_without_pawns    = get(vec, c);
		for (auto& w : complexity) w = S(0, vec[c++]);

		for (int r{ RANK_2 }; r <= RANK_7; ++r) connect_rank[WHITE][r] = get(vec, c);
		for (int r{ RANK_2 }; r <= RANK_7; ++r)  passed_rank[WHITE][r] = S(vec[c++], 0);
		for (int r{ RANK_2 }; r <= RANK_8; ++r)  shield_rank[WHITE][r] = S(vec[c++], 0);

		for (auto& w : knight_mobility) w = get(vec, c);
		for (auto& w : bishop_mobility) w = get(vec, c);
		for (auto& w :   rook_mobility) w = get(vec, c);
		for (auto& w :  queen_mobility) w = get(vec, c);

		eval::mirror_tables();
	}

	static void display_weights()
	{
		// displaying all evaluation weights

		std::cout << "\npiece value: ";     for (auto& w : piece_value) cout(w);
		std::cout << "\nscale few pawns: "; for (auto& w : scale_few_pawns) std::cout << w << ", ";
		std::cout << "\ntempo: "; std::cout << tempo;

		std::cout << "\nthreat king by check: "; for (auto& w : threat_king_by_check) cout(w);
		std::cout << "\nthreat king by x-ray: "; for (auto& w : threat_king_by_xray)  cout(w);
		std::cout << "\nthreat king weight: ";   for (auto& w : threat_king_weight)   std::cout << w << ", ";
		std::cout << "\nthreat king sum:\n";     for (auto& w : threat_king_sum)      cout(w);
		std::cout <<   "\nweak king sq: "; cout(weak_king_sq);

		std::cout <<   "\npawn psqt:\n"; for (auto& w :   pawn_psq[BLACK]) cout(w);
		std::cout << "\nknight psqt:\n"; for (auto& w : knight_psq[BLACK]) cout(w);
		std::cout << "\nbishop psqt:\n"; for (auto& w : bishop_psq[BLACK]) cout(w);
		std::cout <<   "\nrook psqt:\n"; for (auto& w :   rook_psq[BLACK]) cout(w);
		std::cout <<  "\nqueen psqt:\n"; for (auto& w :  queen_psq[BLACK]) cout(w);
		std::cout <<   "\nking psqt:\n"; for (auto& w :   king_psq[BLACK]) cout(w);

		std::cout << "\nbishop pair: ";           cout(bishop_pair);
		std::cout << "\nbishop color pawns: ";    cout(bishop_color_pawns);
		std::cout << "\nbishop trapped: ";        cout(bishop_trapped);
		std::cout << "\nknight outpost: ";        cout(knight_outpost);
		std::cout << "\nknight distance kings: "; for (auto& w : knight_distance_kings) cout(w);
		std::cout << "\nrook on 7th: ";           cout(rook_on_7th);
		std::cout << "\nrook open file: ";        cout(rook_open_file);
		std::cout << "\nthreat pawn: ";           cout(threat_pawn);
		std::cout << "\nthreat minor: ";          cout(threat_minor);
		std::cout << "\nthreat rook: ";           cout(threat_rook);
		std::cout << "\nthreat queen by minor: "; cout(threat_queen_by_minor);
		std::cout << "\nthreat queen by rook: ";  cout(threat_queen_by_rook);
		std::cout << "\nthreat piece by pawn: ";  cout(threat_piece_by_pawn);
		std::cout << "\nthreat piece by king: ";  cout(threat_piece_by_king);
		std::cout << "\nisolated: ";              cout(isolated);
		std::cout << "\nbackward: ";              cout(backward);
		std::cout << "\nking dist passed cl: ";   cout(king_dist_passed_cl);
		std::cout << "\nking dist passed cl_x: "; cout(king_dist_passed_cl_x);
		std::cout << "\nking without pawns: ";    cout(king_without_pawns);
		std::cout << "\ncomplexity: ";            for (auto& w : complexity) cout(w);

		std::cout << "\nconnect rank: "; for (auto& w : connect_rank[WHITE]) cout(w);
		std::cout << "\npassed rank: ";  for (auto& w :  passed_rank[WHITE]) cout(w);
		std::cout << "\nshield rank: ";  for (auto& w :  shield_rank[WHITE]) cout(w);

		std::cout << "\nknight mobility:\n"; for (auto& w : knight_mobility) cout(w);
		std::cout << "\nbishop mobility:\n"; for (auto& w : bishop_mobility) cout(w);
		std::cout <<   "\nrook mobility:\n"; for (auto& w :   rook_mobility) cout(w);
		std::cout <<  "\nqueen mobility:\n"; for (auto& w :  queen_mobility) cout(w);
		std::cout << std::endl;
	}

	static double sigmoid(double k, score sc)
	{
		// calculating sigmoid function

		return 1.0 / (1.0 + std::pow(10.0, -k * (double)sc / 400.0));
	}

	static void eval_error_range(const std::vector<tuning_pos>& pos, std::atomic<double>& err, double k, int range_min, int range_max)
	{
		// calculating the average evaluation error of a range of positions for parallel execution

		verify(range_min < range_max);

		kingpawn_hash hash(kingpawn_hash::ALLOCATE_NONE);
		double err_range{};

		for (int i{ range_min }; i < range_max; ++i)
		{
			score sc{ eval::static_eval(pos[i].pos, hash) * (pos[i].pos.cl == WHITE ? 1 : -1) };
			err_range += std::pow(pos[i].result - sigmoid(k, sc), 2);
		}
		err = err + err_range;
	}

	static double eval_error(const std::vector<tuning_pos>& pos, double k, int thread_cnt)
	{
		// calculating the average evaluation error

		verify(!pos.empty());

		std::atomic<double> err{};
		double share{ pos.size() / double(thread_cnt) };
		double range_min{};

		// using multiple threads here because this is the speed-limiting step

		std::vector<std::thread> threads(thread_cnt);
		for (auto& t : threads)
		{
			t = std::thread{ eval_error_range, std::ref(pos), std::ref(err), k,
				int(std::floor(range_min)), int(std::floor(range_min + share)) };
			range_min += share;
		}
		for (auto& t : threads)
			t.join();

		verify(pos.size() - std::size_t(std::floor(range_min)) <= 1);
		return err / double(std::floor(range_min));
	}

	static double optimal_k(const std::vector<tuning_pos>& pos, int thread_cnt)
	{
		// computing the optimal scaling constant K

		verify(!pos.empty());

		double k_best{};
		double err_min{ std::numeric_limits<double>::max() };
		std::cout.precision(k_precision + 1);

		for (int i{}; i <= k_precision; ++i)
		{
			std::cout << "iteration " << i + 1 << ": ...  ";
			double unit{ std::pow(10.0, -i) };
			double range{ unit * 10.0 };
			double k_max{ k_best + range };

			for (double k{ k_best - range }; k <= k_max; k += unit)
			{
				double err{ eval_error(pos, k, thread_cnt) };
				if (err < err_min)
				{
					err_min = err;
					k_best = k;
				}
			}
			std::cout << "K = " << k_best << std::endl;
		}
		return k_best;
	}

	static bool smaller_error(const std::vector<int>& weights, const std::vector<tuning_pos>& pos, double& err_min, double k, int thread_cnt)
	{
		// calculating the new evaluation error & determining if it is smaller

		save_weights(weights);
		double err{ eval_error(pos, k, thread_cnt) };
		if (err <= err_min)
		{
			// checking the new error for sanity

			if (err_min - err > err_min / thread_cnt / 2)
				return false;

			err_min = err;
			return true;
		}
		else return false;
	}

	static void iterate(std::vector<int>& weights, std::vector<tuning_pos>& pos, double k, double err_start, int thread_cnt)
	{
		// doing tuning iterating to minimize the evaluation error

		std::cout << "\ntuning ..." << std::endl;
		double err_min{ err_start };

		for (int i{ 1 }; true; ++i)
		{
			std::cout << "\niteration " << i << std::endl;
			double err_curr{ err_min };
			for (auto& w : weights)
			{
				int save_weight{ w };
				bool improve{ false };
				w += 1;
				if (smaller_error(weights, pos, err_min, k, thread_cnt))
					improve = true;
				else
				{
					w -= 2;
					improve = smaller_error(weights, pos, err_min, k, thread_cnt);
				}
				if (!improve)
					w = save_weight;
			}

			verify(err_min <= err_curr);
			std::cout << "evaluation error: " << err_min << std::endl;
			save_weights(weights);
			display_weights();

			if (err_min == err_curr)
			{
				std::cout << "\n\nevaluation error   : " << err_start << " -> " << err_min;
				break;
			}
		}
	}
}

void tune::evaluation(std::vector<std::string>& tuning_files, int thread_cnt)
{
	// tuning the evaluation parameters with positions from the tuning files

	std::vector<tuning_pos> pos{};
	thread_cnt = std::max(1, thread_cnt);
	std::cout << "threads available: " << thread_cnt << std::endl;
	load_files(tuning_files, pos);

	// loading current evaluation weights

	std::vector<int> weights{};
	weights.reserve(weight_cnt);
	load_weights(weights);
	display_weights();
	std::cout << std::endl << "weights to tune : " << weights.size() << std::endl;
	if (weights.size() != weight_cnt)
		std::cout << "warning: weight count may not be complete" << std::endl;

	// computing optimal scaling constant K

	std::cout << "computing optimal K ..." << std::endl;
	double k{ optimal_k(pos, thread_cnt) };

	// computing the evaluation error to start with and estimating iteration time

	std::cout << "computing error ..." << std::endl;
	auto start_time{ clock() };
	double err_start{ eval_error(pos, k, thread_cnt) };
	auto finish_time{ clock() };
	std::cout.precision(7);
	std::cout << "start evaluation error : " << err_start << std::endl;
	std::cout << "expected iteration time: ";
	std::cout << (finish_time - start_time) * weights.size() * 2 / 1000 << " sec" << std::endl;

	// iterating until the evaluation error is minimized

	iterate(weights, pos, k, err_start, thread_cnt);
	std::cout << "\ntime: " << (clock() - start_time) / 1000 / 60 << " min";
	std::cout << "\ntuning finished" << std::endl;
}

#endif
