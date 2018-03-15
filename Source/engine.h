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


#pragma once

#include <vector>

#include "position.h"
#include "main.h"

// interface between the communication protocol and the engine

class engine
{
public:

	// handling FRC

	static bool chess960;

	// board state

	static void new_game(board &pos);
	static void new_move(board &pos, uint32 move);
	static void set_position(board &pos, std::string fen);
	
	static const std::string startpos;

	// updating game state

	static void reset_game(const board &pos);
	static void save_move(const board &pos);

	static int move_cnt;
	static int move_offset;

	static uint64 quiet_hash[];

	// initialising magic bitboard move generation

	static void init_magic();

	// filling attack-tables

	static void init_attack();

	// handling opening book

	static void init_book();
	static uint32 get_book_move(board &pos);
	static std::string get_book_name();
	static void new_book(std::string new_name);

	static bool use_book;
	static bool book_move;
	static bool best_book_line;

	// initialising filestream

	static void init_path(char *argv[]);

	// controlling the main transposition table

	static void new_hash_size(int size);
	static void clear_hash();

	static int hash_size;

	// generating Zobrist hash keys

	static void init_zobrist();

	// controlling search

	static uint32 start_searching(board &pos, uint64 time, uint32 &ponder);
	static void search_summary();

	static bool stop;
	static bool ponder;
	static bool infinite;

	static int contempt[2];
	static int multipv;

	static struct search_limit
	{
		std::vector<uint32> moves;
		uint64 nodes;
		int depth;
		int mate;
	} limit;

	// adjusting time calculations

	static int overhead;

	// initialising & debugging evaluation

	static void init_eval();
	static void eval(board &pos);
};