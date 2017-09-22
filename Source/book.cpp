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


// this file is based on PolyGlot by Fabien Letouzey

#include <random>

#include "attack.h"
#include "hash.h"
#include "logfile.h"
#include "engine.h"
#include "movegen.h"
#include "book.h"

std::string book::book_name{ "monolith.bin" };

std::ifstream book::book_file;
int book::book_size;

namespace
{
	uint64 read_int(std::ifstream& file, int size)
	{
		char buf[8];
		file.read(buf, size);
		assert(file.good());
		assert(size >= 2 && size <= 8);

		uint64 n{ 0 };
		for (int i{ 0 }; i < size; ++i)
			n = (n << 8) + static_cast<unsigned char>(buf[i]);
		return n;
	}

	bool is_fully_legal(pos &board, uint32 move)
	{
		movegen list(board);
		pos saved(board);
		if (list.is_pseudolegal(move))
		{
			board.new_move(move);
			bool is_legal{ attack::check(board, board.not_turn, board.pieces[KINGS] & board.side[board.not_turn]) != 0ULL };
			board = saved;

			return is_legal;
		}
		return false;
	}

	static_assert(PROMO_KNIGHT == 12, "promo flag computation");
	static_assert(PROMO_QUEEN  == 15, "promo flag computation");
}

bool book::open()
{
	if (book_file.is_open())
		book_file.close();

	std::string path{ log_file::get_path() + book_name };

	book_file.open(path, std::ifstream::in | std::ifstream::binary);
	if (!book_file.is_open())
		return false;

	book_file.seekg(0, std::ios::end);
	book_size = static_cast<int>(book_file.tellg()) / 16;
	if (book_size == 0)
		return false;

	book_file.seekg(0, std::ios::beg);
	if (!book_file.good())
		return false;

	return true;
}

int book::find_key(const uint64 &key)
{
	// binary search, finding the leftmost entry

	int left{ 0 };
	int right{ book_size - 1 };
	int mid;
	book_entry entry;

	assert(left <= right);
	while (left < right)
	{
		mid = (left + right) / 2;
		assert(mid >= left && mid < right);

		read_entry(entry, mid);

		if (key <= entry.key)
			right = mid;
		else
			left = mid + 1;
	}

	assert(left == right);
	read_entry(entry, left);

	return (entry.key == key) ? left : book_size;
}

void book::read_entry(book_entry &entry, int idx)
{
	// retrieving the found book entry

	assert(idx >= 0 && idx < book_size);
	assert(book_file.is_open());

	book_file.seekg(idx * 16, std::ios_base::beg);
	assert(book_file.good());

	entry.key = read_int(book_file, 8);
	entry.move = static_cast<uint16>(read_int(book_file, 2));
	entry.count = static_cast<uint16>(read_int(book_file, 2));
	entry.n = static_cast<uint16>(read_int(book_file, 2));
	entry.sum = static_cast<uint16>(read_int(book_file, 2));
}

uint32 book::get_move(pos &board, bool best_line)
{
	srand(static_cast<unsigned int>(time(0)));

	if (book_file.is_open() && book_size != 0)
	{
		int score{ 0 }, best_score{ 0 };
		int count{ 0 }, best_count{ 0 };

		ASSERT(board.key == zobrist::to_key(board));

		book_entry entry;

		for (int i{ find_key(board.key) }; i < book_size; ++count, ++i)
		{
			read_entry(entry, i);
			if (entry.key != board.key) break;

			score = entry.count;

			if (best_line)
			{
				if (score > best_score)
				{
					best_score = score;
					best_count = count;
				}
			}
			else
			{
				best_score += score;
				if (rand() % best_score < score)
					best_count = count;
			}

			assert(score > 0 && entry.move != 0);
		}
		if (count)
		{
			read_entry(entry, find_key(board.key) + best_count);
			assert(board.key == entry.key);

			uint16 best_move{ entry.move };

			// encoding the move

			int sq1{ 7 - ((best_move & 0x1c0) >> 6) + ((best_move & 0xe00) >> 6) };
			int sq2{ 7 - (best_move & 0x007) + (best_move & 0x038) };
			int flag{ NONE };
			int piece{ board.piece_sq[sq1] };
			int victim{ board.piece_sq[sq2] };

			if (piece == PAWNS)
			{
				// setting enpassant flag

				if (victim == NONE && abs(sq1 - sq2) % 8 != 0)
				{
					flag = ENPASSANT;
					victim = PAWNS;
				}

				// setting doublepush flag

				if (abs(sq1 - sq2) == 16)
					flag = DOUBLEPUSH;

				// setting promotion flag

				auto promo{ (best_move & 0x7000) >> 12 };
				if (promo)
					flag = 11 + promo;
			}

			// setting castling flag

			else if (piece == KINGS)
			{
				if (sq1 == E1 && sq2 == H1) sq2 = G1, flag = CASTLING::WHITE_SHORT;
				else if (sq1 == E8 && sq2 == H8) sq2 = G8, flag = CASTLING::BLACK_SHORT;
				else if (sq1 == E1 && sq2 == A1) sq2 = C1, flag = CASTLING::WHITE_LONG;
				else if (sq1 == E8 && sq2 == A8) sq2 = C8, flag = CASTLING::BLACK_LONG;
			}

			uint32 encoded_move{ encode(sq1, sq2, flag, piece, victim, board.turn) };

			if (is_fully_legal(board, encoded_move))
				return encoded_move;
		}
	}

	engine::use_book = false;
	return NO_MOVE;
}
