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


#pragma once

#include <sstream>

#include "position.h"
#include "main.h"

// handling the universal chess interface communication protocol

namespace uci
{
	// options & parameters

	const std::string version_number{ "1.0" };
	const std::string startpos{ "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1" };

	extern bool chess960;

	extern int move_count;
	extern int move_offset;
	extern uint64 quiet_hash[];

	extern bool use_book;
	extern bool bookmove;

	extern int hash_size;

	extern int thread_count;

	extern struct syzygy_settings
	{
		std::string path;
		int depth;
		int pieces;
		bool rule50;
	} syzygy;

	extern bool stop;
	extern bool ponder;
	extern bool infinite;

	extern int contempt[2];
	extern int multipv;

	extern struct search_limit
	{
		std::vector<uint32> moves;
		int64 nodes;
		int depth;
		int mate;
	} limit;

	extern int overhead;

	// connecting to PolyGlot opening books

	void open_book();

	// resetting the game status

	void set_position(board &pos, std::string fen);
	void new_game(board &pos);

	// starting the main communication loop

	void loop();
}