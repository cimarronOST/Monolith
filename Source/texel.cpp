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


// all credits for the Texel tuning approach go to Peter Österlund:
// https://www.chessprogramming.org/Texel%27s_Tuning_Method

#include "texel.h"

#if defined(TUNE)

#include <atomic>
#include <thread>
#include <cmath>

#include "thread.h"
#include "search.h"
#include "board.h"
#include "misc.h"
#include "eval.h"
#include "types.h"

using namespace eval;

namespace texel
{
	constexpr int k_precision{ 5 };
	constexpr int weight_cnt{ 645 };
	int calc_errors{};

	struct texel_position { board pos; double result; };
}

namespace texel
{
	void next(square &sq)
	{
		// skipping the left half of the piece-square-tables because they gets mirrored anyway

		sq += (sq - 3) % 8 ? 1 : 5;
	}

	bool open_file(std::ifstream &stream, const std::string &epd_file)
	{
		// opening the epd-file containing the tuning positions with results

		if (stream.is_open())
			stream.close();

		std::string path{ filesystem::path + epd_file };
		stream.open(path, std::ifstream::in);
		return stream.is_open();
	}

	void load_pos(std::ifstream &stream, std::vector<texel_position> &texel_pos)
	{
		// loading the positions from the epd-file
		// getting everything ready for the quiescence-search first

		board pos{};
		thread_pool threads(1, pos);
		search::p_variation pv{};

		// reading the file

		std::string line{};
		while (std::getline(stream, line))
		{
			texel_position curr_pos{};
			auto fen_end{ line.find_last_of(' ', std::string::npos) };

			std::string result{ line.substr(fen_end + 1, std::string::npos) };
			if (result == "1/2-1/2")  curr_pos.result = 0.5;
			else if (result == "1-0") curr_pos.result = 1.0;
			else if (result == "0-1") curr_pos.result = 0.0;

			// quiescence-search resolves non-quiet positions

			curr_pos.pos.parse_fen(line.substr(0, fen_end));
			pv.cnt = 0;
			search::qsearch(*threads.thread[0], curr_pos.pos, pv, curr_pos.pos.check(), 1, 0, -score::mate, score::mate);
			for (depth dt{}; dt < pv.cnt; ++dt)
				curr_pos.pos.new_move(pv.mv[dt]);

			texel_pos.push_back(curr_pos);
		}
	}

	void load_weights(std::vector<int>& weights)
	{
		// loading all the evaluation weights into a vector
		
		for (auto p{ pawn };   p <= queen; p = p + 1) weights.push_back(piece_value[mg][p]);
		for (auto p{ knight }; p <= queen; p = p + 1) weights.push_back(piece_value[eg][p]);
		for (auto p{ knight }; p <= queen; p = p + 1) weights.push_back(phase_value[p]);

		for (auto p{ knight }; p <= queen; p = p + 1) weights.push_back(threat_king_by_check[p]);
		for (auto p{ knight }; p <= queen; p = p + 1) weights.push_back(threat_king_weight[p]);
		for (int i{ 7 }; i < 60; ++i)                 weights.push_back(threat_king[i]);
		weights.push_back(weak_king_sq);

		for (auto& s : pawn_psq[white])   for (auto sq{ h2 }; sq <= a7; next(sq)) weights.push_back(s[sq]);
		for (auto& s : knight_psq[white]) for (auto sq{ h1 }; sq <= a8; next(sq)) weights.push_back(s[sq]);
		for (auto& s : bishop_psq[white]) for (auto sq{ h1 }; sq <= a8; next(sq)) weights.push_back(s[sq]);
		for (auto& s : rook_psq[white])   for (auto sq{ h1 }; sq <= a8; next(sq)) weights.push_back(s[sq]);
		for (auto& s : queen_psq[white])  for (auto sq{ h1 }; sq <= a8; next(sq)) weights.push_back(s[sq]);
		for (auto& s : king_psq[white])   for (auto sq{ h1 }; sq <= a8; next(sq)) weights.push_back(s[sq]);

		for (auto& w : bishop_pair)           weights.push_back(w);
		for (auto& w : bishop_color_pawns)    weights.push_back(w);
		for (auto& w : bishop_trapped)        weights.push_back(w);
		for (auto& w : knight_outpost)        weights.push_back(w);
		for (auto& w : knight_distance_kings) weights.push_back(w);
		for (auto& w : major_on_7th)          weights.push_back(w);
		for (auto& w : rook_open_file)        weights.push_back(w);
		for (auto& w : threat_pawn)           weights.push_back(w);
		for (auto& w : threat_minor)          weights.push_back(w);
		for (auto& w : threat_rook)           weights.push_back(w);
		for (auto& w : threat_queen_by_minor) weights.push_back(w);
		for (auto& w : threat_queen_by_rook)  weights.push_back(w);
		for (auto& w : threat_piece_by_pawn)  weights.push_back(w);
		for (auto& w : isolated)              weights.push_back(w);
		for (auto& w : backward)              weights.push_back(w);
		for (auto& w : king_distance_cl)      weights.push_back(w);
		for (auto& w : king_distance_cl_x)    weights.push_back(w);
		for (auto& w : complexity)            weights.push_back(w);

		for (int r{ rank_2 }; r <= rank_7; ++r) weights.push_back(connected[white][mg][r]);
		for (int r{ rank_2 }; r <= rank_7; ++r) weights.push_back(connected[white][eg][r]);
		for (int r{ rank_2 }; r <= rank_7; ++r) weights.push_back(passed_rank[white][r]);
		for (int r{ rank_2 }; r <= rank_7; ++r) weights.push_back(passed_rank[white][r]);
		for (int r{ rank_2 }; r <= rank_8; ++r) weights.push_back(shield_rank[white][r]);
		for (int r{ rank_4 }; r <= rank_6; ++r) weights.push_back(storm_rank[white][r]);

		for (auto& s : knight_mobility) for (auto& w : s) weights.push_back(w);
		for (auto& s : bishop_mobility) for (auto& w : s) weights.push_back(w);
		for (auto& s : rook_mobility)   for (auto& w : s) weights.push_back(w);
		for (auto& s : queen_mobility)  for (auto& w : s) weights.push_back(w);
	}

	void save_weights(const std::vector<int>& weights)
	{
		// copying the tuned evaluation weights back to the evaluation

		int c{};

		for (auto p{ pawn };   p <= queen; p = p + 1) piece_value[mg][p] = weights[c++];
		for (auto p{ knight }; p <= queen; p = p + 1) piece_value[eg][p] = weights[c++];
		for (auto p{ knight }; p <= queen; p = p + 1) phase_value[p] = weights[c++];

		for (auto p{ knight }; p <= queen; p = p + 1) threat_king_by_check[p] = weights[c++];
		for (auto p{ knight }; p <= queen; p = p + 1) threat_king_weight[p] = weights[c++];
		for (int i{ 7 }; i < 60; ++i)                 threat_king[i] = weights[c++];
		weak_king_sq = weights[c++];

		for (auto& s : pawn_psq[white])   for (auto sq{ h2 }; sq <= a7; next(sq)) { s[sq] = weights[c]; s[type::sq_flip(sq)] = weights[c++]; }
		for (auto& s : knight_psq[white]) for (auto sq{ h1 }; sq <= a8; next(sq)) { s[sq] = weights[c]; s[type::sq_flip(sq)] = weights[c++]; }
		for (auto& s : bishop_psq[white]) for (auto sq{ h1 }; sq <= a8; next(sq)) { s[sq] = weights[c]; s[type::sq_flip(sq)] = weights[c++]; }
		for (auto& s : rook_psq[white])   for (auto sq{ h1 }; sq <= a8; next(sq)) { s[sq] = weights[c]; s[type::sq_flip(sq)] = weights[c++]; }
		for (auto& s : queen_psq[white])  for (auto sq{ h1 }; sq <= a8; next(sq)) { s[sq] = weights[c]; s[type::sq_flip(sq)] = weights[c++]; }
		for (auto& s : king_psq[white])   for (auto sq{ h1 }; sq <= a8; next(sq)) { s[sq] = weights[c]; s[type::sq_flip(sq)] = weights[c++]; }

		for (auto& w : bishop_pair)           w = weights[c++];
		for (auto& w : bishop_color_pawns)    w = weights[c++];
		for (auto& w : bishop_trapped)        w = weights[c++];
		for (auto& w : knight_outpost)        w = weights[c++];
		for (auto& w : knight_distance_kings) w = weights[c++];
		for (auto& w : major_on_7th)          w = weights[c++];
		for (auto& w : rook_open_file)        w = weights[c++];
		for (auto& w : threat_pawn)           w = weights[c++];
		for (auto& w : threat_minor)          w = weights[c++];
		for (auto& w : threat_rook)           w = weights[c++];
		for (auto& w : threat_queen_by_minor) w = weights[c++];
		for (auto& w : threat_queen_by_rook)  w = weights[c++];
		for (auto& w : threat_piece_by_pawn)  w = weights[c++];
		for (auto& w : isolated)              w = weights[c++];
		for (auto& w : backward)              w = weights[c++];
		for (auto& w : king_distance_cl)      w = weights[c++];
		for (auto& w : king_distance_cl_x)    w = weights[c++];
		for (auto& w : complexity)            w = weights[c++];

		for (int r{ rank_2 }; r <= rank_7; ++r) connected[white][mg][r] = weights[c++];
		for (int r{ rank_2 }; r <= rank_7; ++r) connected[white][eg][r] = weights[c++];
		for (int r{ rank_2 }; r <= rank_7; ++r) passed_rank[white][r] = weights[c++];
		for (int r{ rank_2 }; r <= rank_8; ++r) shield_rank[white][r] = weights[c++];
		for (int r{ rank_4 }; r <= rank_6; ++r) storm_rank[white][r] = weights[c++];

		for (auto& s : knight_mobility) for (auto& w : s) w = weights[c++];
		for (auto& s : bishop_mobility) for (auto& w : s) w = weights[c++];
		for (auto& s : rook_mobility)   for (auto& w : s) w = weights[c++];
		for (auto& s : queen_mobility)  for (auto& w : s) w = weights[c++];

		eval::mirror_tables();
	}

	void display_weights()
	{
		// displaying all evaluation weights

		std::cout << "piece value mg: ";   for (auto& w : piece_value[mg]) std::cout << w << ", ";
		std::cout << "\npiece value eg: "; for (auto& w : piece_value[eg]) std::cout << w << ", ";
		std::cout << "\nphase value: ";    for (auto& w : phase_value)     std::cout << w << ", ";

		std::cout << "\nthreat king by check: "; for (auto& w : threat_king_by_check) std::cout << w << ", ";
		std::cout << "\nthreat king weight: ";   for (auto& w : threat_king_weight)   std::cout << w << ", ";
		std::cout << "\nthreat king:\n";         for (auto& w : threat_king)          std::cout << w << ", ";
		std::cout << "\nweak king sq: "; std::cout << weak_king_sq;

		std::cout << "\npawn psqt:\n";
		for (auto& s : pawn_psq[white])   { for (auto& w : s) std::cout << w << ", "; std::cout << "\n"; }
		std::cout << "knight psqt:\n";
		for (auto& s : knight_psq[white]) { for (auto& w : s) std::cout << w << ", "; std::cout << "\n"; }
		std::cout << "bishop psqt:\n";
		for (auto& s : bishop_psq[white]) { for (auto& w : s) std::cout << w << ", "; std::cout << "\n"; }
		std::cout << "rook psqt:\n";
		for (auto& s : rook_psq[white])   { for (auto& w : s) std::cout << w << ", "; std::cout << "\n"; }
		std::cout << "queen psqt:\n";
		for (auto& s : queen_psq[white])  { for (auto& w : s) std::cout << w << ", "; std::cout << "\n"; }
		std::cout << "king psqt:\n";
		for (auto& s : king_psq[white])   { for (auto& w : s) std::cout << w << ", "; std::cout << "\n"; }

		std::cout << "bishop pair: ";             for (auto& w : bishop_pair)           std::cout << w << ", ";
		std::cout << "\nbishop color pawns: ";    for (auto& w : bishop_color_pawns)    std::cout << w << ", ";
		std::cout << "\nbishop trapped: ";        for (auto& w : bishop_trapped)        std::cout << w << ", ";
		std::cout << "\nknight outpost: ";        for (auto& w : knight_outpost)        std::cout << w << ", ";
		std::cout << "\nknight distance kings: "; for (auto& w : knight_distance_kings) std::cout << w << ", ";
		std::cout << "\nmajor on 7th: ";          for (auto& w : major_on_7th)          std::cout << w << ", ";
		std::cout << "\nrook open file: ";        for (auto& w : rook_open_file)        std::cout << w << ", ";
		std::cout << "\nthreat pawn: ";           for (auto& w : threat_pawn)           std::cout << w << ", ";
		std::cout << "\nthreat minor: ";          for (auto& w : threat_minor)          std::cout << w << ", ";
		std::cout << "\nthreat rook: ";           for (auto& w : threat_rook)           std::cout << w << ", ";
		std::cout << "\nthreat queen by minor: "; for (auto& w : threat_queen_by_minor) std::cout << w << ", ";
		std::cout << "\nthreat queen by rook: ";  for (auto& w : threat_queen_by_rook)  std::cout << w << ", ";
		std::cout << "\nthreat piece by pawn: ";  for (auto& w : threat_piece_by_pawn)  std::cout << w << ", ";
		std::cout << "\nisolated: ";              for (auto& w : isolated)              std::cout << w << ", ";
		std::cout << "\nbackward: ";              for (auto& w : backward)              std::cout << w << ", ";
		std::cout << "\nking distance cl: ";      for (auto& w : king_distance_cl)      std::cout << w << ", ";
		std::cout << "\nking distance cl x: ";    for (auto& w : king_distance_cl_x)    std::cout << w << ", ";
		std::cout << "\ncomplexity: ";            for (auto& w : complexity)            std::cout << w << ", ";

		std::cout << "\nconnected mg: "; for (auto& w : connected[white][mg]) std::cout << w << ", ";
		std::cout << "\nconnected eg: "; for (auto& w : connected[white][eg]) std::cout << w << ", ";
		std::cout << "\npassed rank: ";  for (auto& w : passed_rank[white])   std::cout << w << ", ";
		std::cout << "\nshield rank: ";  for (auto& w : shield_rank[white])   std::cout << w << ", ";
		std::cout << "\nstorm rank: ";   for (auto& w : storm_rank[white])    std::cout << w << ", ";

		std::cout << "\nknight mobility:\n";
		for (auto& s : knight_mobility) { for (auto& w : s) std::cout << w << ", "; std::cout << "\n"; }
		std::cout << "bishop mobility:\n";
		for (auto& s : bishop_mobility) { for (auto& w : s) std::cout << w << ", "; std::cout << "\n"; }
		std::cout << "rook mobility:\n";
		for (auto& s : rook_mobility)   { for (auto& w : s) std::cout << w << ", "; std::cout << "\n"; }
		std::cout << "queen mobility:\n";
		for (auto& s : queen_mobility)  { for (auto& w : s) std::cout << w << ", "; std::cout << "\n"; }
	}

	double sigmoid(double k, score sc)
	{
		// calculating sigmoid function

		return 1.0 / (1.0 + std::pow(10.0, -k * sc / 400.0));
	}

	void eval_error_range(const std::vector<texel_position>& texel_pos, std::atomic<double>& error, double k, int range_min, int range_max)
	{
		// calculating the average evaluation error of a range

		verify(range_min < range_max);

		kingpawn_hash hash(kingpawn_hash::allocate_none);
		double error_range{};

		for (int i{ range_min }; i < range_max; ++i)
		{
			score sc{ eval::static_eval(texel_pos[i].pos, hash) * (texel_pos[i].pos.cl == white ? 1 : -1) };
			error_range += std::pow(texel_pos[i].result - sigmoid(k, sc), 2);
		}
		error = error + error_range;
	}

	double eval_error(const std::vector<texel_position>& texel_pos, double k, int thread_cnt)
	{
		// calculating the average evaluation error

		verify(!texel_pos.empty());

		std::atomic<double> error{};
		double share{ texel_pos.size() / double(thread_cnt) };
		double range_min{};

		// using multiple threads here because this is the speed-limiting step

		std::vector<std::thread> threads(thread_cnt);
		for (auto& t : threads)
		{
			t = std::thread{ eval_error_range, std::ref(texel_pos), std::ref(error), k,
				int(std::floor(range_min)), int(std::floor(range_min + share) - 1) };
			range_min += share;
		}
		for (auto& t : threads)
			t.join();

		verify(std::size_t(std::floor(range_min)) == texel_pos.size());
		return error / double(texel_pos.size());
	}

	double optimal_k(const std::vector<texel_position>& texel_pos, int thread_cnt)
	{
		// computing the optimal scaling constant K

		verify(!texel_pos.empty());

		double k_best{};
		double error_min{ std::numeric_limits<double>::max() };
		std::cout.precision(k_precision + 1);

		for (int i{}; i <= k_precision; ++i)
		{
			std::cout << "iteration " << i + 1 << ": ...  ";
			double unit{ std::pow(10.0, -i) };
			double range{ unit * 10.0 };
			double k_max{ k_best + range };

			for (double k{ std::max(k_best - range, 0.0) }; k <= k_max; k += unit)
			{
				double error{ eval_error(texel_pos, k, thread_cnt) };
				if (error < error_min)
				{
					error_min = error;
					k_best = k;
				}
			}
			std::cout << "K = " << k_best << std::endl;
		}
		return k_best;
	}

	bool smaller_error(const std::vector<int>& weights, const std::vector<texel_position>& texel_pos, double& error_min, double k, int thread_cnt)
	{
		// calculating the new evaluation error & determining if it is smaller

		save_weights(weights);
		double error{ eval_error(texel_pos, k, thread_cnt) };
		if (error <= error_min)
		{
			// checking the new error for sanity

			if (error_min - error > error_min / thread_cnt / 2)
			{
				calc_errors += 1;
				return false;
			}
			error_min = error;
			return true;
		}
		else
			return false;
	}
}

void texel::tune(std::string& epd_file, int thread_cnt)
{
	// tuning the evaluation parameters with positions from the epd_file
	// opening the epd_file first

	std::ifstream stream{};
	if (open_file(stream, epd_file))
		std::cout << "\nloading epd-file ..." << std::endl;
	else
	{
		std::cout << "\nwarning: failed to open \"" << epd_file << "\", path = " << filesystem::path << std::endl;
		return;
	}

	// loading positions from the epd_file

	std::vector<texel_position> texel_pos{};
	thread_cnt = std::max(1, thread_cnt);
	load_pos(stream, texel_pos);
	std::cout << "entries loaded  : " << texel_pos.size() << std::endl;
	std::cout.precision(2);
	std::cout << "memory allocated: " << (sizeof(texel_position) * texel_pos.size() >> 20) << " MB" << std::endl;

	// loading current evaluation weights

	std::vector<int> weights{};
	weights.reserve(weight_cnt);
	load_weights(weights);
	std::cout << "weights to tune : " << weights.size() << std::endl;
	if (weights.size() != weight_cnt)
		std::cout << "warning: weight count may not be complete" << std::endl;

	// computing optimal scaling constant K

	std::cout << "computing optimal K ..." << std::endl;
	double k{ optimal_k(texel_pos, thread_cnt) };

	// computing the evaluation error to start with

	std::cout << "computing error ..." << std::endl;
	auto start_time{ clock() };
	double error_start{ eval_error(texel_pos, k, thread_cnt) };
	auto finish_time{ clock() };
	std::cout.precision(5);
	std::cout << "start evaluation error : " << error_start << std::endl;

	// estimating iteration time

	std::cout << "expected iteration time: ";
	std::cout << (finish_time - start_time) * weights.size() * 2 / 1000 << " sec" << std::endl;

	// starting with the tuning-iterations

	std::cout << "\ntuning ..." << std::endl;
	double error_min{ error_start };
	calc_errors = 0;

	for (int i{ 1 }; true; ++i)
	{
		std::cout << "\niteration " << i << std::endl;
		double error_curr{ error_min };
		for (auto& w : weights)
		{
			int save_weight{ w };
			bool improve{ false };

			w += 1;
			if (smaller_error(weights, texel_pos, error_min, k, thread_cnt))
				improve = true;
			else
			{
				w -= 2;
				improve = smaller_error(weights, texel_pos, error_min, k, thread_cnt);
			}
			if (!improve)
				w = save_weight;
		}

		verify(error_min <= error_curr);
		std::cout << "evaluation error: " << error_min << std::endl;
		save_weights(weights);
		display_weights();

		if (error_min == error_curr)
			break;
	}

	// finishing tuning

	std::cout << "\n\ntime: " << (clock() - start_time) / 1000 / 60 << " min";
	std::cout << "\ncalculation errors : " << calc_errors;
	std::cout << "\nevaluation error   : " << error_start << " -> " << error_min;
	std::cout << "\ntuning finished" << std::endl;
}

#endif
