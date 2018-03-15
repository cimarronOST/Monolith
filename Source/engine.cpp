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


#include "zobrist.h"
#include "trans.h"
#include "eval.h"
#include "search.h"
#include "book.h"
#include "stream.h"
#include "magic.h"
#include "attack.h"
#include "engine.h"

const std::string engine::startpos
{ "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1" };

// values depending on the UCI 'setoption' command

int engine::hash_size{ 128 };
int engine::multipv{ 1 };
int engine::overhead{ 0 };
int engine::contempt[]{ 0, 0 };

bool engine::ponder{ false };
bool engine::chess960{ false };
bool engine::use_book{ true };
bool engine::best_book_line{ false };

// values depending on the UCI 'go' command

bool engine::infinite{ false };

engine::search_limit engine::limit;

// values depending on various other things

int engine::move_cnt{ 0 };
int engine::move_offset{ 0 };

uint64 engine::quiet_hash[256]{ };

bool engine::stop{ true };
bool engine::book_move{ true };

// main transposition table

namespace hash
{
	trans table(engine::hash_size);
}

void engine::new_game(board &pos)
{
	// responding to the UCI 'ucinewgame' command

	book_move = { use_book ? true : false };
	hash::table.clear();
	set_position(pos, startpos);
}

void engine::new_move(board &pos, uint32 move)
{
	pos.new_move(move);
	save_move(pos);
}

void engine::set_position(board &pos, std::string fen)
{
	// reacting to the UCI 'position' command

	pos.parse_fen(fen);
	reset_game(pos);
}

void engine::reset_game(const board &pos)
{
	move_cnt = 0;
	move_offset = 0;
	for (auto &h : quiet_hash) h = 0ULL;
	quiet_hash[move_offset] = pos.key;
}

void engine::save_move(const board &pos)
{
	// keeping track of the half move count & transpositions to detect repetitions

	move_offset = { pos.half_move_cnt ? move_offset + 1 : 0 };
	move_cnt += 1;
	assert(move_offset <= pos.half_move_cnt);

	quiet_hash[move_offset] = pos.key;
}

void engine::init_magic()
{
	magic::index_table();
}

void engine::init_attack()
{
	attack::fill_tables();
}

void engine::init_book()
{
	book_move = book::open();
}

uint32 engine::get_book_move(board &pos)
{
	return book::get_move(pos, best_book_line);
}

std::string engine::get_book_name()
{
	return book::name;
}

void engine::new_book(std::string new_name)
{
	// executing the UCI setoption 'Book File' command

	book::name = new_name;
	init_book();
}

void engine::init_path(char *argv[])
{
	// initialising the directory path for book- & logfile

	filestream::set_path(argv);
	filestream::open();
}

void engine::new_hash_size(int size)
{
	// executing the UCI setoption 'Hash' command

	hash_size = hash::table.create(size);
}

void engine::clear_hash()
{
	// executing the UCI setoption 'Clear Hash' command

	hash::table.clear();
}

void engine::init_zobrist()
{
	zobrist::init_keys();
}

uint32 engine::start_searching(board &pos, uint64 time, uint32 &ponder)
{
	// starting the search after the UCI 'go' command

	analysis::reset();
	return search::id_frame(pos, time, ponder);
}

void engine::search_summary()
{
	// outputting some statistics from the previous search
	// called by the 'summary' command

	analysis::summary();
}

void engine::init_eval()
{
	eval::fill_tables();
}

void engine::eval(board &pos)
{
	// doing a itemised evaluation of the current position
	// called by the 'eval' command for debugging purpose

	eval::itemise_eval(pos);
}