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


#include "engine.h"
#include "search.h"
#include "stream.h"
#include "chronos.h"
#include "position.h"
#include "bench.h"

// sets of positions to analyse and test performance and correctness

namespace
{
	struct position
	{
		std::string fen;
		std::string info;
		int max_depth;
	};

	const position perft_pos[]
	{
		{ "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -", "119060324", 6 },
		{ "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -", "193690690", 5 },
		{ "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ -", "89941194", 5 },
		{ "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - -", "178633661", 7 },
		{ "8/5p2/8/2k3P1/p3K3/8/1P6/8 b - -", "64451405", 8 },
		{ "r1k1r2q/p1ppp1pp/8/8/8/8/P1PPP1PP/R1K1R2Q w KQkq - 0 1", "172843489", 6 },
	};

	const position search_pos[]
	{
		{ "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -", "startpos", 11 },
		{ "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -", "kiwipete: bm Bxa6", 11 },
		{ "rn1qk2r/p1pnbppp/bp2p3/3pN3/2PP4/1P4P1/P2BPPBP/RN1QK2R w KQkq -", "silent but deadly: bm Nxf7", 11 },
		
		{ "2k3r1/4b3/4P3/7B/p6K/P3p3/5n1Q/3q4 w - -", "mate test: mate in 0", 1 },
		{ "4k3/4P1p1/4K1P1/2p5/1pP5/1P1N2B1/8/8 b - -", "draw test: stalemate in 0", 1 },
		{ "8/1kn5/pn6/P6P/6r1/5K2/2r5/8 w - - 99 120", "draw test: 50-move-rule-draw in 0", 5 },

		{ "8/3k1p2/8/8/3q3P/5K2/8/6q1 w - -", "depth limit test: mate in -1", lim::depth, },
		{ "8/8/pppppppK/NBBR1NRp/nbbrqnrP/PPPPPPPk/8/Q7 w - -", "qsearch limit test: mate in 1 bm Qh1", 2 },

		{ "5B2/6P1/1p6/8/1N6/kP6/2K5/8 w - -", "hakmem 70: mate in 3 bm g8N", 8 },
		{ "8/8/p3R3/1p5p/1P5p/6rp/5K1p/7k w - -", "zugzwang & nullmove test: mate in 7 bm Re1", 17 },

		{ "7K/8/k1P5/7p/8/8/8/8 w - -", "reti study: insu.-mat.-draw in 6 bm Kg7", 13 },
		{ "8/1kn5/pn6/P6P/6r1/5K2/2r5/8 w - - 97 115", "draw test: 50-move-rule-draw in 2 bm Ke3", 10 },
		{ "k7/P5R1/7P/8/8/6P1/4r3/5K2 b - -", "razoring test: rep.-draw in 7 bm Rf2", 15 },
		{ "6R1/P2k4/r7/5N1P/r7/p7/7K/8 w - -", "djaja study: unsolved rep.-draw in ? bm Nh6", 11 },
		
		{ "r3k2r/8/8/8/8/8/8/R3K2R w KQkq -", "tt test: mate in 7 bm Rxh8+", 16 },
		{ "3k4/1p6/1P1K4/2P5/8/8/8/8 w - -", "tt test: mate in 8 bm Ke6", 16 },
		{ "4k3/8/8/8/8/8/R7/4K3 w - -", "tt test: mate in 12 bm Ra7", 30 },
		{ "8/2k5/4P3/8/1K6/8/8/8 w - -", "tt test: mate in 12 bm Kc5", 28 },
		{ "8/8/8/1k6/8/8/8/RK6 w - -", "tt test rook & king: mate in 13 bm Kc2", 25 },
		{ "4k3/8/8/8/8/8/4P3/4K3 w - -", "tt test fine 70: mate in 22 bm Kd2", 33 },
		{ "8/k7/3p4/p2P1p2/P2P1P2/8/8/K7 w - -", "tt test lasker-reichhelm: mate in 32 bm Kb1", 35 }
	};
}

void bench::perft(std::string gen)
{
	// performance test, asserting the correctness and speed of the move-generator

	filestream::open();
	board pos;

	auto mode{ gen == "pseudo" ? PSEUDO : LEGAL };
	analysis::reset();
	sync::cout << "\nperft " << (gen != "pseudo" ? "legal" : "pseudolegal") << ":\n";

	for (auto &p : perft_pos)
	{
		sync::cout << "\nfen=\"" << p.fen << "\"\n\n";
		engine::set_position(pos, p.fen);

		analysis::root_perft(pos, p.max_depth, mode);
		sync::cout << ">>>>> " << p.max_depth << ": " << p.info << std::endl;
	}

	sync::cout << std::endl;
	analysis::summary();
}

void bench::search()
{
	// running a limited search on the above set of various positions

	filestream::open();
	uint64 movetime{ lim::movetime };
	uint32 ponder{ };
	int cnt{ };
	engine::infinite = true;
	engine::stop = false;
	board pos;

	analysis::reset();
	sync::cout << "\nposition benchmark:\n";

	for (auto &p : search_pos)
	{
		sync::cout << "\nposition " << ++cnt << " " << p.info << "\n" << std::endl;

		engine::new_game(pos);
		engine::set_position(pos, p.fen);
		engine::limit.depth = p.max_depth;
		search::id_frame(pos, movetime, ponder);
	}

	sync::cout << std::endl;
	analysis::summary();
	engine::infinite = false;
	engine::stop = true;
}