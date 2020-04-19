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


#pragma once

#include "move.h"
#include "thread.h"
#include "polyglot.h"
#include "trans.h"
#include "types.h"
#include "main.h"

// interface of the Universal Chess Interface (UCI) communication protocol

namespace uci
{
	// fixed parameters

	const std::string version_number{ "2" };
	const std::string startpos{ "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1" };

	// alterable parameters

	extern bool use_abdada;
	extern bool chess960;
	extern bool log;
	extern bool stop;
	extern bool ponder;
	extern bool infinite;
	extern bool use_book;
	extern book bk;

	extern int thread_cnt;
	extern int mv_cnt;
	extern int mv_offset;
	extern std::array<key64, 256> game_hash;

	extern std::size_t multipv;
	extern std::size_t hash_size;
	extern milliseconds overhead;

	extern struct search_limit
	{
		std::vector<move> searchmoves;
		milliseconds movetime;
		int64 nodes;
		depth dt;
		depth mate;
		void set_infinite();
	} limit;

	extern struct syzygy_settings
	{
		std::string path;
		depth dt;
		int pieces;
	} syzygy;

	// main transposition hash-table

	extern trans hash_table;

	// communication loop

	void loop();

	// output of current search information

	void info_iteration(sthread& thread, int mv_cnt);
	void info_bound(sthread& thread, int pv_n, score sc, bound bd);
	void info_currmove(sthread& thread, int pv_n, move mv, int mv_n);
	void info_bestmove(std::tuple<move, move> mv);
}
