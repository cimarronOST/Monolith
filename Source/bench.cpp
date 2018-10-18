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


#include <fstream>

#include "thread.h"
#include "uci.h"
#include "search.h"
#include "stream.h"
#include "position.h"
#include "bench.h"

namespace
{
	// extending the fen type to contain additional information

	struct fen_extended
	{
		std::string fen;
		std::string info;
		int max_depth;
	};

	// set of perft positions

	std::vector<fen_extended> perft_pos
	{
		{ "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -", "119060324", 6 },
		{ "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -", "193690690", 5 },
		{ "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ -", "89941194", 5 },
		{ "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - -", "178633661", 7 },
		{ "8/5p2/8/2k3P1/p3K3/8/1P6/8 b - -", "64451405", 8 },
		{ "r1k1r2q/p1ppp1pp/8/8/8/8/P1PPP1PP/R1K1R2Q w KQkq - 0 1", "172843489", 6 },
	};

	// set of various search positions to analyze & benchmark the search behavior

	std::vector<fen_extended> search_pos
	{
		{ "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -", "start position", 11 },
		{ "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -", "kiwipete: bm e2a6", 11 },
		{ "rn1qk2r/p1pnbppp/bp2p3/3pN3/2PP4/1P4P1/P2BPPBP/RN1QK2R w KQkq -", "silent but deadly: bm e5f7", 11 },

		{ "2k3r1/4b3/4P3/7B/p6K/P3p3/5n1Q/3q4 w - -", "immediate mate test: mate 0", 1 },
		{ "4k3/4P1p1/4K1P1/2p5/1pP5/1P1N2B1/8/8 b - -", "immediate draw test: stalemate 0", 1 },
		{ "8/1kn5/pn6/P6P/6r1/5K2/2r5/8 w - - 99 120", "immediate draw test: 50-move-rule-draw 0", 3 },

		{ "8/3k1p2/8/8/3q3P/5K2/8/6q1 w - -", "depth limit test: KPvKQQP mate -1", lim::depth, },
		{ "8/8/pppppppK/NBBR1NRp/nbbrqnrP/PPPPPPPk/8/Q7 w - -", "qsearch limit test: mate 1 bm a1h1", 3 },

		{ "5B2/6P1/1p6/8/1N6/kP6/2K5/8 w - -", "hakmem 70: KBNPPvKP mate 3 bm g7g8n", 6 },
		{ "8/8/p3R3/1p5p/1P5p/6rp/5K1p/7k w - -", "zugzwang & null-move test: mate 7 bm e6e1", 17 },

		{ "7K/8/k1P5/7p/8/8/8/8 w - -", "reti study: KPvKP material-draw 6 bm h8g7", 15 },
		{ "8/1kn5/pn6/P6P/6r1/5K2/2r5/8 w - - 97 115", "draw test: 50-move-rule-draw 2 bm f3e3", 12 },
		{ "k7/P5R1/7P/8/8/6P1/4r3/5K2 b - -", "razoring test: KRPPPvKR repetition-draw 7 bm e2f2", 15 },
		{ "6R1/P2k4/r7/5N1P/r7/p7/7K/8 w - -", "djaja study: repetition-draw ? bm f5h6", 11 },

		{ "r3k2r/8/8/8/8/8/8/R3K2R w KQkq -", "tt test: KRRvKRR mate 7 bm h1h8 a1a8", 17 },
		{ "3k4/1p6/1P1K4/2P5/8/8/8/8 w - -", "tt test: KPPvKP mate 8 bm d6e6", 16 },
		{ "4k3/8/8/8/8/8/R7/4K3 w - -", "tt test: KRvK mate 12 bm a2a7", 30 },
		{ "8/2k5/4P3/8/1K6/8/8/8 w - -", "tt test: KPvK mate 12 bm b4c5", 28 },
		{ "8/8/8/1k6/8/8/8/RK6 w - -", "tt test: KRvK mate 13 bm b1c2", 30 },
	    { "8/8/8/p7/Kb1k4/8/8/8 w - - 2 67", "tt test: KvKBP mate -13 bm a4b3", 31 },
		{ "4k3/8/8/8/8/8/4P3/4K3 w - -", "tt test fine 70: KPvK mate 22 bm e1d2 e1f2", 36 },
		{ "8/k7/3p4/p2P1p2/P2P1P2/8/8/K7 w - -", "tt test lasker-reichhelm: KPPPvKPP mate 32 bm a1b1", 35 },
	};
}

namespace get
{
	// parsing the Extended Position Description (EPD) format

	std::string fen(std::string &input)
	{
		return input.substr(0, input.find("bm") - 1);
	}

	std::string bm(std::string &input)
	{
		auto bm_begin{ input.find("bm") + 3 };
		return input.substr(bm_begin, input.find_first_of(';') - bm_begin);
	}

	std::string id(std::string &input)
	{
		auto id_begin{ input.find_first_of('"') + 1 };
		return input.substr(id_begin, input.find_last_of('"') - id_begin);
	}
}

void bench::perft(std::string gen)
{
	// performance test, asserting the correctness and speed of the move-generator

	filestream::open();
	board pos;

	auto mode{ gen == "pseudo" ? PSEUDO : LEGAL };
	analysis::reset();
	sync::cout << "running perft with " << (gen != "pseudo" ? "legal" : "pseudolegal") << " move generator\n";

	for (auto &p : perft_pos)
	{
		sync::cout << "\nfen=\"" << p.fen << "\"\n\n";
		uci::set_position(pos, p.fen);

		analysis::perft(pos, p.max_depth, mode);
		sync::cout << ">>>>> " << p.max_depth << ": " << p.info << std::endl;
	}

	sync::cout << std::endl;
	analysis::summary();
}

void bench::search(std::string &filename, int64 &time)
{
	// running a limited search on the set of various positions above

	sync::cout << "running benchmark\n";

	board pos;
	thread_pool threads(uci::thread_count, pos);
	int64 movetime{ lim::movetime };
	int count{};

	threads.start_all();
	filestream::open();
	analysis::reset();
	uci::infinite = true;
	uci::limit.nodes = lim::nodes;

	// running an external set of positions contained in <filename.epd> for <time> milliseconds instead

	std::fstream stream(filestream::fullpath + filename);
	if (stream.is_open())
	{
		std::string input;
		search_pos.clear();
		uci::infinite = false;
		movetime = time;

		while(std::getline(stream, input))
			search_pos.push_back({ get::fen(input), get::id(input) + " bm " + get::bm(input), lim::depth });
	}

	// starting the benchmark

	for (auto &p : search_pos)
	{
		sync::cout << "\nposition " << ++count << ": " << p.info << "\nfen=\"" << p.fen << "\"" << std::endl;

		uci::new_game(pos);
		uci::set_position(pos, p.fen);
		uci::limit.depth = p.max_depth;
		uci::stop = false;		
		search::start(threads, movetime);
	}

	sync::cout << std::endl;
	analysis::summary();
	uci::infinite = false;
	uci::stop = true;
}