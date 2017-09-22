/*
  Monolith 0.3  Copyright (C) 2017 Jonas Mayr

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

#include "position.h"
#include "chronos.h"
#include "main.h"

// interface between the communication protocol and the engine

class engine
{
public:

	// board state

	static void new_game(pos &board);
	static void new_move(pos &board, uint32 move);
	static void parse_fen(pos &board, std::string fen);
	
	static const std::string startpos;

	// updating game state

	static void reset_game();
	static void save_move(const pos &board, uint32 move);

	static int move_cnt;
	static uint32 movelist[];
	static uint64 hashlist[];

	// move generation

	static void init_magic();
	static void init_movegen();

	// opening book

	static void init_book();
	static uint32 get_book_move(pos &board);
	static std::string get_book_name();
	static void new_book(std::string new_name);

	static bool use_book;
	static bool best_book_line;

	// file path

	static void init_path(char *argv[]);

	// manipulating the transposition table

	static void new_hash_size(int size);
	static void clear_hash();

	static int hash_size;

	// controlling search

	static uint32 start_searching(pos &board, chronos &chrono, uint32 &ponder);
	static void stop_ponder();

	static bool stop;
	static bool infinite;

	static uint64 nodes;
	static int depth;
	static int contempt;

	// evaluation

	static void init_eval();
	static void eval(pos &board);
};
