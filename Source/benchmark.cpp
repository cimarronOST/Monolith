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


#include "benchmark.h"

#ifdef DEBUG

#include "chronos.h"
#include "engine.h"
#include "search.h"
#include "files.h"

namespace
{
	struct unit
	{
		string fen;
		int depth_max;
		string info;
	};

	const unit perft_pos[]
	{
		{ "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -", 6, "119060324" },
		{ "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -", 5, "193690690" },
		{ "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", 5, "89941194" },
		{ "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - -", 7, "178633661" },
		{ "8/5p2/8/2k3P1/p3K3/8/1P6/8 b - -", 8, "64451405" }
	};
	const unit search_pos[]
	{
		{ "2k3r1/4b3/4P3/7B/p6K/P3p3/5n1Q/3q4 w - -", 1, "is mate" },
		{ "4k3/4P1p1/4K1P1/2p5/1pP5/1P1N2B1/8/8 b - -", 1, "is stalemate" },
		{ "8/1kn5/pn6/P6P/6r1/5K2/2r5/8 w - - 99 95", 10, "is draw" },
		{ "5B2/6P1/1p6/8/1N6/kP6/2K5/8 w - -", 9, "hakmem 70 | mate in 3 | bm g8N" },
		{ "8/8/p3R3/1p5p/1P5p/6rp/5K1p/7k w - -", 16, "zugzwang | mate in 7 | bm Re1"},

		{ "7K/8/k1P5/7p/8/8/8/8 w - -", 13, "reti study | insu.ma.-draw | draw in 6 | bm Kg7" },
		{ "8/1kn5/pn6/P6P/6r1/5K2/2r5/8 w - - 97 115", 7, "50-move-rule-draw | draw in 2 | bm Ke3" },
		{ "k7/P5R1/7P/8/8/6P1/4r3/5K2 b - -", 14, "rep.-draw | draw in 7 | bm Rf2" },
		{ "6R1/P2k4/r7/5N1P/r7/p7/7K/8 w - -", 11, "djaja study | rep.-draw | bm Nh6" }, // unsolved

		{ "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -", 9, "startpos" },
		{ "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -", 6, "kiwipete | midgame search" },
		{ "rn1qk2r/p1pnbppp/bp2p3/3pN3/2PP4/1P4P1/P2BPPBP/RN1QK2R w KQkq -", 8, "silent but deadly | midgame search | bm Nxf7" },

		{ "8/k7/3p4/p2P1p2/P2P1P2/8/8/K7 w - -", 28, "lasker-reichhelm | tt-test | mate in 32 | bm a1b1;" }, // unsolved
		{ "4k3/8/8/8/8/8/4P3/4K3 w - -", 20, "fine 70 | tt-test | mate in 22 | bm Kd2" }, // unsolved
		{ "8/8/8/1k6/8/8/8/RK6 w - -", 21, "rook + king | tt-test | mate in 13 | bm Kc2"}, // unsolved
		{ "3k4/1p6/1P1K4/2P5/8/8/8/8 w - -", 20, "tt-test | mate in 8 | bm Ke6"}
	};

	int s_depth;
	int s_movetime;
}

void benchmark::analysis(const string type)
{
	pos board;
	chronos chrono;
	files::open();
	
	if (type == "perft")
	{
		timer time;
		analysis::reset();

		log::cout << "\nBENCHMARK PERFT" << "\n=========================\n";

		for (auto &p : perft_pos)
		{
			log::cout << p.fen << endl;
			engine::parse_fen(board, chrono, p.fen);

			analysis::root_perft(board, 1, p.depth_max);
			log::cout << "         " << p.info << endl;
		}

		analysis::summary(time);
	}

	else if (type == "search")
	{
		timer time;
		analysis::reset();

		s_depth = engine::depth;
		s_movetime = chrono.movetime;

		log::cout << "\nBENCHMARK SEARCH" << "\n=========================\n";

		for (auto &s : search_pos)
		{
			log::cout << endl << s.info << "\n-------------------------\n";

			engine::parse_fen(board, chrono, s.fen);
			engine::depth = s.depth_max;
			engine::stop = false;

			search::id_frame(board, chrono);
		}

		analysis::summary(time);

		engine::depth = s_depth;
		chrono.movetime = s_movetime;
	}

	else std::cerr << "error: <option>" << endl;
}

#endif
