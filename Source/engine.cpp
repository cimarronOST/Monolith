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


#include "hash.h"
#include "evaluation.h"
#include "search.h"
#include "book.h"
#include "game.h"
#include "files.h"
#include "movegen.h"
#include "bitboard.h"
#include "engine.h"

int engine::hash_size{ 128 };
int engine::depth;
bool engine::use_book;
bool engine::stop;

namespace
{
	// transposition table

	tt table(engine::hash_size);
}

void engine::new_game(pos &board, chronos &chrono)
{
	parse_fen(board, chrono, startpos);
	use_book = true;
}
void engine::new_move(pos &board, uint64 &sq1_64, uint64 &sq2_64, uint8 flag)
{
	bb::bitscan(sq1_64);
	int sq1{ static_cast<int>(bb::lsb()) };
	bb::bitscan(sq2_64);
	int sq2{ static_cast<int>(bb::lsb()) };

	uint16 move{ encode(sq1, sq2, flag) };
	board.new_move(move);

	assert(sq2_64 & board.side[board.turn ^ 1]);

	game::save_move(board, move);
}
void engine::parse_fen(pos &board, chronos &chrono, string fen)
{
	game::reset();

	use_book = false;
	depth = lim::depth;

	chrono.set_movetime(lim::movetime);
	table.clear();

	board.parse_fen(fen);
}

uint32 engine::bitscan(uint64 board)
{
	bb::bitscan(board);
	return bb::lsb();
}

void engine::init_movegen()
{
	movegen::init();

	magic::init();
	magic::init_ray(ROOK);
	magic::init_ray(BISHOP);
	magic::init_king();
	magic::init_knight();
}

void engine::init_path(string path)
{
	files::set_path(path);
	if(!files::open())
		std::cerr << "warning: not able to access path \'" + path + "\'" << endl;
}
void engine::init_rand()
{
	srand(static_cast<unsigned int>(time(0)));
}

void engine::init_book()
{
	if (book::open())
		use_book = true;
	else
		use_book = false;
}
uint16 engine::get_book_move(pos &board)
{
	return book::get_move(board);
}

void engine::new_hash_size(int size)
{
	hash_size = table.create(size);
}

uint16 engine::alphabeta(pos &board, chronos &chrono)
{
	stop = false;
	analysis::reset();
	return search::id_frame(board, chrono);
}

void engine::init_eval()
{
	eval::init();
}
