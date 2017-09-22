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


#include "hash.h"
#include "evaluation.h"
#include "search.h"
#include "book.h"
#include "logfile.h"
#include "magic.h"
#include "movegen.h"
#include "engine.h"

// internal values

const std::string engine::startpos
{
	"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
};

bool engine::stop{ true };
int engine::move_cnt;

uint32 engine::movelist[lim::period];
uint64 engine::hashlist[lim::period];

// independent values

int engine::hash_size{ 128 };
int engine::contempt{ 0 };
bool engine::best_book_line{ false };

// values dependent on UCI input

bool engine::infinite{ false };
int engine::depth;
uint64 engine::nodes;

// values dependent on file placement

bool engine::use_book;

namespace
{
	// transposition table

	tt table(engine::hash_size);
}

void engine::new_game(pos &board)
{
	use_book = true;
	table.reset();

	parse_fen(board, startpos);
}

void engine::new_move(pos &board, uint32 move)
{
	board.new_move(move);
	save_move(board, move);
}

void engine::parse_fen(pos &board, std::string fen)
{
	reset_game();
	board.parse_fen(fen);
}

void engine::reset_game()
{
	move_cnt = 0;

	for (auto &m : movelist) m = NO_MOVE;
	for (auto &h : hashlist) h = 0ULL;
}

void engine::save_move(const pos &board, uint32 move)
{
	movelist[move_cnt] = move;
	hashlist[move_cnt] = board.key;
	move_cnt += 1;
}

void engine::init_magic()
{
	magic::init();
}

void engine::init_movegen()
{
	movegen::init_ray(ROOK);
	movegen::init_ray(BISHOP);
	movegen::init_king();
	movegen::init_knight();
}

void engine::init_path(char *argv[])
{
	log_file::set_path(argv);
	log_file::open();
}

void engine::init_book()
{
	// assuming path is already established

	if (book::open())
		use_book = true;
	else
		use_book = false;
}

uint32 engine::get_book_move(pos &board)
{
	return book::get_move(board, best_book_line);
}

std::string engine::get_book_name()
{
	return book::book_name;
}

void engine::new_book(std::string new_name)
{
	book::book_name = new_name;
	init_book();
}

void engine::new_hash_size(int size)
{
	hash_size = table.create(size);
}

void engine::clear_hash()
{
	table.reset();
}

uint32 engine::start_searching(pos &board, chronos &chrono, uint32 &ponder)
{
	analysis::reset();
	return search::id_frame(board, chrono, ponder);
}

void engine::stop_ponder()
{
	infinite = false;
	search::stop_ponder();
}

void engine::init_eval()
{
	eval::init();
}

void engine::eval(pos &board)
{
	// doing a static evaluation of the current position
	// for debugging purpose

	log::cout << "total evaluation: " << eval::static_eval(board)
		<< " cp (" << (board.turn == WHITE ? "white's" : "black's") << " point of view)"
		<< std::endl;
}
