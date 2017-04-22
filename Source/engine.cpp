/*
  Monolith 0.1  Copyright (C) 2017 Jonas Mayr

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
#include "convert.h"
#include "game.h"
#include "files.h"
#include "movegen.h"
#include "bitboard.h"
#include "engine.h"

int engine::hash_size{ 128 };
int engine::depth;
bool engine::play_with_book;
bool engine::stop;

void engine::new_game(pos &board, chronos &chrono)
{
	game::reset();
	board.parse_fen(startpos);

	play_with_book = true;
	depth = lim::depth;

	chrono.set_movetime(lim::movetime);
	hashing::tt_clear();
}
void engine::new_move(pos &board, uint64 &sq1_64, uint64 &sq2_64, uint8 flag)
{
	int control{ game::moves / 2 + 1 };
	if (game::moves != control)
		game::game_str += std::to_string(control) + ". ";

	game::game_str += conv::bit_to_san(board, sq1_64, sq2_64, flag) + " ";

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

	play_with_book = false;
	depth = lim::depth;

	chrono.set_movetime(lim::movetime);
	hashing::tt_clear();

	board.parse_fen(fen);
}

uint32 engine::bitscan(uint64 board)
{
	bb::bitscan(board);
	return bb::lsb();
}

void engine::init_movegen()
{
	//// to be called only once

	movegen::init();

	magic::init();
	magic::init_ray(rook);
	magic::init_ray(bishop);
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
		play_with_book = true;
	else
		play_with_book = false;
}
uint16 engine::get_book_move(pos &board)
{
	return book::get_move(board);
}

void engine::init_hash(int size)
{
	delete_hash();
	hashing::tt_create(size);
}
void engine::delete_hash()
{
	hashing::tt_delete();
}

uint16 engine::alphabeta(pos &board, chronos &chrono)
{
	analysis::reset();
	return search::id_frame(board, chrono);
}

void engine::init_eval()
{
	eval::init();
}