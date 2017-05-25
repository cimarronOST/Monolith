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


#pragma once

#include "position.h"
#include "chronos.h"
#include "main.h"

class engine
{
public:
	// position

	static void new_game(pos &board, chronos &chrono);
	static void new_move(pos &board, uint64 &sq1, uint64 &sq2, uint8 flag);
	static void parse_fen(pos &board, chronos &chrono, string fen);

	// bitboard

	static uint32 bitscan(uint64 board);

	// movegen

	static void init_movegen();

	// book

	static bool use_book;
	static void init_book();
	static uint16 get_book_move(pos &board);

	// files

	static void init_path(string path);

	// random

	static void init_rand();

	// thread

	static bool stop;

	// hash

	static void new_hash_size(int size);
	static int hash_size;

	// search

	static uint16 alphabeta(pos &board, chronos &chrono);
	static int depth;

	// eval

	static void init_eval();
};
