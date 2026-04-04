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


#include <string>
#include <vector>
#include <iostream>
#include <algorithm>
#include <fstream>
#include <chrono>

#include "main.h"
#include "types.h"
#include "thread.h"
#include "uci.h"
#include "search.h"
#include "misc.h"
#include "time.h"
#include "movegen.h"
#include "board.h"
#include "bench.h"

namespace fen
{
	struct position
	{
		std::string info;
		std::string best_mv;
		std::string fen;
		depth dt;
	};

	// set of various search positions to analyze & benchmark the search behavior

	std::vector<position> search
	{ {
		{ "start position",    "0000", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -", 17 },
		{ "kiwipete",          "e2a6", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -", 18 },
		{ "silent but deadly", "e5f7", "rn1qk2r/p1pnbppp/bp2p3/3pN3/2PP4/1P4P1/P2BPPBP/RN1QK2R w KQkq -", 21 },

		{ "find immediate mate: mate 0",         "0000", "2k3r1/4b3/4P3/7B/p6K/P3p3/5n1Q/3q4 w - -", 1 },
		{ "find immediate draw: stalemate 0",    "0000", "4k3/4P1p1/4K1P1/2p5/1pP5/1P1N2B1/8/8 b - -", 1 },
		{ "find immediate draw: 50-move-draw 0", "0000", "8/1kn5/pn6/P6P/6r1/5K2/2r5/8 w - - 99 120", 3 },

		{ "find mate: hakmem 70: KBNPPvKP mate 3",   "g7g8n", "5B2/6P1/1p6/8/1N6/kP6/2K5/8 w - -", 10 },
		{ "find mate: zugzwang & null-move: mate 7", "e6e1",  "8/8/p3R3/1p5p/1P5p/6rp/5K1p/7k w - -", 24 },
		{ "find mate: mate 12",                      "g3h5",  "2R5/p4pkp/2br1qp1/5P2/3p4/1P2Q1NP/P1P3P1/6K1 w - -", 20 },
		{ "find mate: mate 15",                      "g1a1",  "8/6p1/1pp5/k7/1p6/1P6/6pK/6Qb w - -", 30 },
		{ "find mate: mate 19",                      "g1d4",  "8/8/5p2/6Q1/8/p1K5/p2pp2p/k5B1 w - -", 25 },

		{ "find draw: 50-move-rule-draw 2",                  "f3e3", "8/1kn5/pn6/P6P/6r1/5K2/2r5/8 w - - 97 115", 25 },
		{ "find draw: reti study: KPvKP material-draw 6",    "h8g7", "7K/8/k1P5/7p/8/8/8/8 w - -", 15 },
		{ "find draw: razoring: KRPPPvKR repetition-draw 7", "e2f2", "k7/P5R1/7P/8/8/6P1/4r3/5K2 b - -", 15 },
		{ "find draw: djaja study: repetition-draw ?",       "f5h6", "6R1/P2k4/r7/5N1P/r7/p7/7K/8 w - -", 15 },

		{ "test chess960",                      "0000", "rbkrb1qn/1pp1ppp1/3pn2p/pP6/8/4N1P1/P1PPPP1P/RBKRB1QN w DAda - 0 9", 17 },
		{ "test chess324",                      "0000", "rnqbknbr/pppppppp/8/8/8/8/PPPPPPPP/RQBBKNNR w KQkq - 0 1", 17 },
		{ "test high depth: repetition draw ?", "0000", "k1b5/1p1p4/pP1Pp3/K2pPp2/1P1p1P2/3P1P2/5P2/8 w - -", 75 },
		{ "test depth limit: KPvKQQP mate -1",  "0000", "8/3k1p2/8/8/3q3P/5K2/8/6q1 w - -", lim::dt },
		{ "test seldepth limit",                "0000", "4r3/1p5k/6p1/1b1p2p1/1p1P4/1P2QPP1/P4K1P/8 b - - 0 32", 29 },
		{ "test qsearch explosion: mate 1",     "a1h1", "8/8/pppppppK/NBBR1NRp/nbbrqnrP/PPPPPPPk/8/Q7 w - -", 3 },

		{ "test tt: mate score: mate 7",  "g4g7", "r1b1qr1k/5ppp/2p1p3/p1PpP3/P2N1PQB/2P1R3/6PP/3n2K1 w - -", 16 },
		{ "test tt: KRRvKRR mate 7", "h1h8 a1a8", "r3k2r/8/8/8/8/8/8/R3K2R w KQkq -", 25 },
		{ "test tt: KPPvKP mate 8",       "d6e6", "3k4/1p6/1P1K4/2P5/8/8/8/8 w - -", 25 },
		{ "test tt: KRvK mate 12",        "a2a7", "4k3/8/8/8/8/8/R7/4K3 w - -", 35 },
		{ "test tt: KPvK mate 12",        "b4c5", "8/2k5/4P3/8/1K6/8/8/8 w - -", 28 },
		{ "test tt: KRvK mate 13",        "b1c2", "8/8/8/1k6/8/8/8/RK6 w - -", 35 },
		{ "test tt: KvKBP mate -13",      "a4b3", "8/8/8/p7/Kb1k4/8/8/8 w - - 2 67", 35 },
		{ "test tt: fine 70: KPvK mate 22",           "e1d2 e1f2", "4k3/8/8/8/8/8/4P3/4K3 w - -", 30 },
		{ "test tt: lasker-reichhelm: KPPPPvKPPP mate 32", "a1b1", "8/k7/3p4/p2P1p2/P2P1P2/8/8/K7 w - -", 35 },

		{ "test syzygy 5-pieces: KBNvKP dtz 107 cursed win",        "a4c5", "8/8/2K5/7B/N7/1k6/1p6/8 w - -", 1 },
		{ "test syzygy 5-pieces: KPPvKP dtz 104 blessed loss", "b6c6 b6c7", "8/8/1k2P2K/6P1/8/3p4/8/8 b - -", 1 },
		{ "test syzygy 6-pieces: KQPvKQP dtz 100 loss",             "f3e3", "q7/8/8/P7/3Q4/5K2/p7/4k3 w - -", 1 },
		{ "test syzygy 7-pieces: KRPPvKRP dtz 99 win",              "h5g6", "2r5/8/8/2pP3K/2P2k2/3R4/8/8 w - -", 1 }
	} };
}

namespace epd
{
	// parsing the Extended Position Description (EPD) format

	static std::string fen(const std::string& input)
	{
		return input.substr(0, input.find("bm") - 1);
	}

	static std::string bm(const std::string& input)
	{
		auto bm_begin{ input.find("bm") + 3 };
		return input.substr(bm_begin, input.find_first_of(';') - bm_begin);
	}

	static std::string id(const std::string& input)
	{
		auto id_begin{ input.find_first_of('"') + 1 };
		return input.substr(id_begin, input.find_last_of('"') - id_begin);
	}
}

namespace bench
{
	template<mode md>
	int64 perft_search(board &pos, depth dt)
	{
		// a performance test (perft) search completes the whole search tree for the depth specified
		// without any pruning or cutoffs
		// essentially it is a correctness & speed test of the move-generator and the make-move function

		if (dt == 0)
			return 1;
		int64 nodes{};
		gen<md> list(pos);
		list.gen_all();

		for (int i{}; i < list.cnt.mv; ++i)
		{
			pos.new_move(list.mv[i]);
			if constexpr (md == mode::PSEUDOLEGAL)
				if (!pos.legal())
				{
					pos = list.pos;
					continue;
				}
			nodes += perft_search<md>(pos, dt - 1);
			pos = list.pos;
		}
		return nodes;
	}
}

template void bench::perft<mode::LEGAL>(board, depth);
template void bench::perft<mode::PSEUDOLEGAL>(board, depth);

template<mode md>
void bench::perft(board pos, depth dt_max)
{
	// starting perft

	chronometer chrono{};
	int64 all_nodes{};
	std::cout.precision(3);

	for (depth dt{ 1 }; dt <= dt_max; ++dt)
	{
		std::cout << "perft " << dt << ": ";

		int64 nodes{ perft_search<md>(pos, dt) };
		auto time { chrono.elapsed() };

		all_nodes += nodes;
		std::cout << nodes
			<< " time " << time
			<< " nps " << std::fixed << milliseconds(all_nodes) / std::max(time, milliseconds(1)) << " kN/s"
			<< std::endl;
	}
}

void bench::search(const std::string& filename, const milliseconds& time)
{
	// running a limited benchmark search on a set of positions

	std::cout << "running benchmark\n";
	verify(uci::mv_offset == 0);

	board pos{};
	thread_pool threads(uci::thread_cnt, pos);
	timemanage::move_time mv_time{ lim::movetime, milliseconds(0) };

	uci::limit.nodes = lim::nodes;
	search::bench = 0;
	chronometer::reset_hit_threshold();
	threads.start_all();

	// the positions contained in fen::search can be skipped by specifying an external set of positions in filename
	// only .epd files are supported
	// each position is then searched with the amount of time given

	std::fstream stream(filesystem::path + filename);
	if (stream.is_open())
	{
		std::string input{};
		fen::search.clear();
		mv_time.target = time;

		while (std::getline(stream, input))
			fen::search.push_back(fen::position{ epd::id(input), epd::bm(input), epd::fen(input), lim::dt });
	}

	// starting the benchmark and measuring the time

	chronometer chrono{};
	int cnt{};
	for (auto& p : fen::search)
	{
		std::cout << "\nposition " << ++cnt << ": " << p.info << " bm " << p.best_mv
			<< "\nfen=\"" << p.fen << "\"" << std::endl;

		pos.parse_fen(p.fen);
		threads.clear_history();
		uci::game_hash[uci::mv_offset] = pos.key.pos;
		uci::hash_table.clear();
		uci::limit.dt = p.dt;
		uci::stop = false;
		
		search::start(threads, mv_time);
	}

	uci::stop = true;
	auto interim{ chrono.elapsed() };
	std::cout
		<< "\ntime  : " << interim << " ms"
		<< "\nnodes : " << search::bench
		<< "\nnps   : " << search::bench / interim.count() << " kN/s"
		<< std::endl;
}