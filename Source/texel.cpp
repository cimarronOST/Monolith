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

 
// all credits for the texel tuning approach go to Peter Österlund:
// https://www.chessprogramming.org/Texel%27s_Tuning_Method

#include "texel.h"

#if defined(TUNE)

#include <atomic>
#include <thread>
#include <fstream>

#include "utilities.h"
#include "position.h"
#include "stream.h"
#include "eval.h"

using namespace eval;

namespace
{
	// speeding up processes

	constexpr int perspective[]{ 1, -1 };
	constexpr int k_precision  { 5 };
	constexpr int weight_count { 567 };

	int calculation_error{};
}

namespace texel
{
	// adding game result to the position-representation

	struct texel_position
	{
		board pos;
		double result;
	};

	std::atomic<double> error_final{};
	int thread_count{ 1 };

	void next(int &sq)
	{
		// skipping the left half of a 64-entry-table (because it gets mirrored anyway)

		if ((sq - 3) % 8 == 0) sq += 4;
		sq += 1;
	}

	bool open_file(std::ifstream &stream, std::string &epd_file)
	{
		// opening the epd-file containing the positions with results

		auto path{ filestream::fullpath + epd_file };
		if (stream.is_open())
			stream.close();

		stream.open(path, std::ifstream::in);
		return stream.is_open();
	}

	void load_positions(std::ifstream &stream, std::vector<texel_position> &texel_pos)
	{
		// loading the positions from the epd-file

		std::string line;
		while (std::getline(stream, line))
		{
			texel_position curr_pos{};
			auto fen_end{ line.find_last_of(' ', std::string::npos) };
			curr_pos.pos.parse_fen(line.substr(0, fen_end));

			auto result{ line.substr(fen_end + 1, std::string::npos) };
			if  (result == "1/2-1/2") curr_pos.result = 0.5;
			else if (result == "1-0") curr_pos.result = 1.0;
			else if (result == "0-1") curr_pos.result = 0.0;

			texel_pos.push_back(curr_pos);
		}
	}

	void load_weights(std::vector<int> &weights)
	{
		// loading all the evaluation weights into <weights>

		for (int p{   PAWNS }; p <= QUEENS; ++p) weights.push_back(piece_value[MG][p]);
		for (int p{ KNIGHTS }; p <= QUEENS; ++p) weights.push_back(piece_value[EG][p]);
		for (int p{ KNIGHTS }; p <= QUEENS; ++p) weights.push_back(phase_value[p]);

		for (auto &s : pawn_psq[  WHITE]) for (int sq{ H2 }; sq <= A7; next(sq)) weights.push_back(s[sq]);
		for (auto &s : knight_psq[WHITE]) for (int sq{ H1 }; sq <= A8; next(sq)) weights.push_back(s[sq]);
		for (auto &s : bishop_psq[WHITE]) for (int sq{ H1 }; sq <= A8; next(sq)) weights.push_back(s[sq]);
		for (auto &s : rook_psq[  WHITE]) for (int sq{ H1 }; sq <= A8; next(sq)) weights.push_back(s[sq]);
		for (auto &s : queen_psq[ WHITE]) for (int sq{ H1 }; sq <= A8; next(sq)) weights.push_back(s[sq]);
		for (auto &s : king_psq[  WHITE]) for (int sq{ H1 }; sq <= A8; next(sq)) weights.push_back(s[sq]);

		for (auto &w : bishop_pair)          weights.push_back(w);
		for (auto &w : knight_outpost)       weights.push_back(w);
		for (auto &w : major_on_7th)         weights.push_back(w);
		for (auto &w : rook_open_file)       weights.push_back(w);
		for (auto &w : pawn_threat)          weights.push_back(w);
		for (auto &w : minor_threat)         weights.push_back(w);
		for (auto &w : queen_threat)         weights.push_back(w);
		for (auto &w : queen_threat_minor)   weights.push_back(w);
		for (auto &w : isolated)             weights.push_back(w);
		for (auto &w : backward)             weights.push_back(w);
		for (auto &w : king_distance_friend) weights.push_back(w);
		for (auto &w : king_distance_enemy)  weights.push_back(w);

		for (int r{ R2 }; r <= R7; ++r) weights.push_back(connected[WHITE][MG][r]);
		for (int r{ R2 }; r <= R7; ++r) weights.push_back(connected[WHITE][EG][r]);
		for (int r{ R2 }; r <= R7; ++r) weights.push_back(passed_rank[WHITE][r]);
		for (int r{ R2 }; r <= R7; ++r) weights.push_back(shield_rank[WHITE][r]);
		for (int r{ R4 }; r <= R6; ++r) weights.push_back(storm_rank[ WHITE][r]);

		for (auto &s : knight_mobility) for (auto &w : s) weights.push_back(w);
		for (auto &s : bishop_mobility) for (auto &w : s) weights.push_back(w);
		for (auto &s : rook_mobility)   for (auto &w : s) weights.push_back(w);
		for (auto &s : queen_mobility)  for (auto &w : s) weights.push_back(w);
	}

	void save_weights(const std::vector<int> &weights)
	{
		// copying the tuned evaluation weights back to the evaluation

		int c{};
		for (int p{   PAWNS }; p <= QUEENS; ++p) piece_value[MG][p] = weights[c++];
		for (int p{ KNIGHTS }; p <= QUEENS; ++p) piece_value[EG][p] = weights[c++];
		for (int p{ KNIGHTS }; p <= QUEENS; ++p) phase_value[p]     = weights[c++];

		for (auto &s : pawn_psq[  WHITE]) for (int sq{ H2 }; sq <= A7; next(sq)) { s[sq] = weights[c]; s[square::flip(sq)] = weights[c++]; }
		for (auto &s : knight_psq[WHITE]) for (int sq{ H1 }; sq <= A8; next(sq)) { s[sq] = weights[c]; s[square::flip(sq)] = weights[c++]; }
		for (auto &s : bishop_psq[WHITE]) for (int sq{ H1 }; sq <= A8; next(sq)) { s[sq] = weights[c]; s[square::flip(sq)] = weights[c++]; }
		for (auto &s : rook_psq[  WHITE]) for (int sq{ H1 }; sq <= A8; next(sq)) { s[sq] = weights[c]; s[square::flip(sq)] = weights[c++]; }
		for (auto &s : queen_psq[ WHITE]) for (int sq{ H1 }; sq <= A8; next(sq)) { s[sq] = weights[c]; s[square::flip(sq)] = weights[c++]; }
		for (auto &s : king_psq[  WHITE]) for (int sq{ H1 }; sq <= A8; next(sq)) { s[sq] = weights[c]; s[square::flip(sq)] = weights[c++]; }

		for (auto &w : bishop_pair)          w = weights[c++];
		for (auto &w : knight_outpost)       w = weights[c++];
		for (auto &w : major_on_7th)         w = weights[c++];
		for (auto &w : rook_open_file)       w = weights[c++];
		for (auto &w : pawn_threat)          w = weights[c++];
		for (auto &w : minor_threat)         w = weights[c++];
		for (auto &w : queen_threat)         w = weights[c++];
		for (auto &w : queen_threat_minor)   w = weights[c++];
		for (auto &w : isolated)             w = weights[c++];
		for (auto &w : backward)             w = weights[c++];
		for (auto &w : king_distance_friend) w = weights[c++];
		for (auto &w : king_distance_enemy)  w = weights[c++];

		for (int r{ R2 }; r <= R7; ++r) connected[WHITE][MG][r] = weights[c++];
		for (int r{ R2 }; r <= R7; ++r) connected[WHITE][EG][r] = weights[c++];
		for (int r{ R2 }; r <= R7; ++r) passed_rank[WHITE][r]   = weights[c++];
		for (int r{ R2 }; r <= R7; ++r) shield_rank[WHITE][r]   = weights[c++];
		for (int r{ R4 }; r <= R6; ++r) storm_rank[ WHITE][r]   = weights[c++];

		for (auto &s : knight_mobility) for (auto &w : s) w = weights[c++];
		for (auto &s : bishop_mobility) for (auto &w : s) w = weights[c++];
		for (auto &s : rook_mobility)   for (auto &w : s) w = weights[c++];
		for (auto &s : queen_mobility)  for (auto &w : s) w = weights[c++];

		eval::mirror_tables();
		assert(c == count::weights);
	}

	void display_weights()
	{
		// displaying all evaluation weights

		sync::cout <<   "piece value mg: "; for (auto &w : piece_value[MG]) sync::cout << w << ", ";
		sync::cout << "\npiece value eg: "; for (auto &w : piece_value[EG]) sync::cout << w << ", ";
		sync::cout << "\nphase value: ";    for (auto &w : phase_value)     sync::cout << w << ", ";

		sync::cout << "\npawn psqt:\n";
		for (auto &s : pawn_psq[  WHITE]) { for (auto &w : s) sync::cout << w << ", "; sync::cout << "\n"; }
		sync::cout << "knight psqt:\n";
		for (auto &s : knight_psq[WHITE]) { for (auto &w : s) sync::cout << w << ", "; sync::cout << "\n"; }
		sync::cout << "bishop psqt:\n";
		for (auto &s : bishop_psq[WHITE]) { for (auto &w : s) sync::cout << w << ", "; sync::cout << "\n"; }
		sync::cout << "rook psqt:\n";
		for (auto &s : rook_psq[  WHITE]) { for (auto &w : s) sync::cout << w << ", "; sync::cout << "\n"; }
		sync::cout << "queen psqt:\n";
		for (auto &s : queen_psq[ WHITE]) { for (auto &w : s) sync::cout << w << ", "; sync::cout << "\n"; }
		sync::cout << "king psqt:\n";
		for (auto &s : king_psq[  WHITE]) { for (auto &w : s) sync::cout << w << ", "; sync::cout << "\n"; }

		sync::cout << "bishop pair: ";            for (auto &w : bishop_pair)          sync::cout << w << ", ";
		sync::cout << "\nknight outpost: ";       for (auto &w : knight_outpost)       sync::cout << w << ", ";
		sync::cout << "\nmajor on 7th: ";         for (auto &w : major_on_7th)         sync::cout << w << ", ";
		sync::cout << "\nrook open file: ";       for (auto &w : rook_open_file)       sync::cout << w << ", ";
		sync::cout << "\npawn threat: ";          for (auto &w : pawn_threat)          sync::cout << w << ", ";
		sync::cout << "\nminor threat: ";         for (auto &w : minor_threat)         sync::cout << w << ", ";
		sync::cout << "\nqueen threat: ";         for (auto &w : queen_threat)         sync::cout << w << ", ";
		sync::cout << "\nqueen threat minor: ";   for (auto &w : queen_threat_minor)   sync::cout << w << ", ";
		sync::cout << "\nisolated: ";             for (auto &w : isolated)             sync::cout << w << ", ";
		sync::cout << "\nbackward: ";             for (auto &w : backward)             sync::cout << w << ", ";
		sync::cout << "\nking distance friend: "; for (auto &w : king_distance_friend) sync::cout << w << ", ";
		sync::cout << "\nking distance enemy: ";  for (auto &w : king_distance_enemy)  sync::cout << w << ", ";

		sync::cout << "\nconnected mg: ";         for (auto &w : connected[WHITE][MG]) sync::cout << w << ", ";
		sync::cout << "\nconnected eg: ";         for (auto &w : connected[WHITE][EG]) sync::cout << w << ", ";
		sync::cout << "\npassed rank: ";          for (auto &w : passed_rank[WHITE])   sync::cout << w << ", ";
		sync::cout << "\nshield rank: ";          for (auto &w : shield_rank[WHITE])   sync::cout << w << ", ";
		sync::cout << "\nstorm rank: ";           for (auto &w : storm_rank[ WHITE])   sync::cout << w << ", ";

		sync::cout << "\nknight mobility:\n";
		for (auto &s : knight_mobility) { for (auto &w : s) sync::cout << w << ", "; sync::cout << "\n"; }
		sync::cout << "bishop mobility:\n";
		for (auto &s : bishop_mobility) { for (auto &w : s) sync::cout << w << ", "; sync::cout << "\n"; }
		sync::cout << "rook mobility:\n";
		for (auto &s : rook_mobility)   { for (auto &w : s) sync::cout << w << ", "; sync::cout << "\n"; }
		sync::cout << "queen mobility:\n";
		for (auto &s : queen_mobility)  { for (auto &w : s) sync::cout << w << ", "; sync::cout << "\n"; }
	}

	double sigmoid(double k, double score)
	{
		// calculating sigmoid function

		return 1.0 / (1.0 + std::pow(10.0, -k * score / 400.0));
	}

	void eval_error_range(const std::vector<texel_position> &texel_pos, double k, int range_min, int range_max)
	{
		// calculating the average evaluation error of a range

		assert(range_min < range_max);

		pawn hash(false);
		double error{};

		for (auto i{ range_min }; i < range_max; ++i)
		{
			error += std::pow(texel_pos[i].result
				- sigmoid(k, eval::static_eval(texel_pos[i].pos, hash) * perspective[texel_pos[i].pos.turn]), 2);
		}
		error_final = error_final + error;
	}

	double eval_error(const std::vector<texel_position> &texel_pos, double k)
	{
		// calculating the average evaluation error

		assert(!texel_pos.empty());

		error_final = 0.0;
		auto share{ texel_pos.size() / static_cast<double>(thread_count) };
		double range_min{};

		// using multiple threads because this is the speed-limiting step

		std::vector<std::thread> threads(thread_count);
		for (auto &t : threads)
		{
			t = std::thread{ eval_error_range, std::ref(texel_pos), k,
				static_cast<int>(std::floor(range_min)), static_cast<int>(std::floor(range_min + share)) };
			range_min += share;
		}
		for (auto &t : threads) t.join();
		
		return error_final / static_cast<double>(texel_pos.size());
	}

	double optimal_k(const std::vector<texel_position> &texel_pos)
	{
		// computing the optimal scaling constant K

		assert(k_precision >= 0);
		assert(texel_pos.size() > 0U);

		double k_best{};
		double error_min{ std::numeric_limits<double>::max() };
		sync::cout.precision(k_precision + 1);

		for (int i{ 0 }; i <= k_precision; ++i)
		{
			sync::cout << "iteration " << i + 1 << ": ...  ";
			auto unit{ std::pow(10.0, -i) };
			auto range{ unit * 10.0 };
			auto k_max{ k_best + range };

			for (auto k{ std::max(k_best - range, 0.0) }; k <= k_max; k += unit)
			{
				auto error{ eval_error(texel_pos, k) };
				if  (error < error_min)
				{
					error_min = error;
					k_best = k;
				}
			}
			sync::cout << "K = " << k_best << std::endl;
		}
		return k_best;
	}

	bool smaller_error(const std::vector<int> &weights, const std::vector<texel_position> &texel_pos, double &error_min, double k)
	{
		// calculating the new evaluation error & determining if it is smaller

		save_weights(weights);
		auto error{ eval_error(texel_pos, k) };
		if  (error <= error_min)
		{
			// checking the new error for sanity

			if (error_min - error > error_min / thread_count / 2)
			{
				calculation_error += 1;
				return false;
			}

			error_min = error;
			return true;
		}
		else
			return false;
	}
}

void texel::tune(std::string &epd_file, int threads)
{
	// tuning the evaluation parameters with positions from the <epd_file>
	// opening the epd-file first
	
	std::ifstream stream;
	if (open_file(stream, epd_file))
		sync::cout << "\nloading epd-file ..." << std::endl;
	else
	{
		sync::cout << "\nwarning: failed to open \"" << epd_file << "\", path = " << filestream::fullpath << std::endl;
		return;
	}

	// loading positions from the epd-file

	std::vector<texel_position> texel_pos;
	thread_count = std::max(1, threads);
	texel_pos.reserve(thread_count);
	load_positions(stream, texel_pos);
	sync::cout << "entries loaded  : " << texel_pos.size() << std::endl;
	sync::cout.precision(2);
	sync::cout << "memory allocated: " << sizeof(texel_position) * texel_pos.size() / 1000000 << " MB" << std::endl;

	// loading current evaluation weights

	std::vector<int> weights;
	weights.reserve(weight_count);
	sync::cout << "loading weights ..." << std::endl;
	load_weights(weights);
	sync::cout << "weights to tune: " << weights.size() << std::endl;
	if (weights.size() != weight_count)
		sync::cout << "warning: weights are not in sync" << std::endl;

	// computing optimal scaling constant K

	sync::cout << "computing optimal K ..." << std::endl;
	auto k{ optimal_k(texel_pos) }; 

	// computing the start-error

	sync::cout << "computing error ..." << std::endl;
	auto start_time { clock() };
	auto error_start{ eval_error(texel_pos, k) };
	auto finish_time{ clock() };
	sync::cout.precision(5);
	sync::cout << "start evaluation error    : " << error_start << std::endl;

	// estimating iteration time

	sync::cout << "minimal time per iteration: ";
	sync::cout << (finish_time - start_time) * weights.size() / 1000 << " sec"<< std::endl;

	// starting with the tuning-iterations

	sync::cout << "\ntuning ..." << std::endl;
	auto error_min{ error_start };
	int iteration{};

	while(true)
	{
		sync::cout << "\niteration " << ++iteration << std::endl;
		auto error_curr{ error_min };
		for (auto &w : weights)
		{
			auto save_weight{ w };
			bool improve{ false };

			w += 1;
			if (smaller_error(weights, texel_pos, error_min, k))
				improve = true;
			else
			{
				w -= 2;
				improve = smaller_error(weights, texel_pos, error_min, k);
			}

			if (!improve)
				w = save_weight;
		}

		assert(error_min <= error_curr);
		sync::cout << "evaluation error: " << error_min << std::endl;
		save_weights(weights);
		display_weights();
		
		if (error_min == error_curr)
			break;
	}

	// finishing tuning

	sync::cout << "\n\ntime: " << (clock() - start_time) / 1000 / 60 << " min";
	sync::cout << "\ncalculation errors : " << calculation_error;
	sync::cout << "\nevaluation error   : " << error_start << " -> " << error_min;
	sync::cout << "\ntuning finished" << std::endl;
}

#endif