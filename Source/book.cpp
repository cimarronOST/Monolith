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


//// this file is based on the book.cpp code from PolyGlot by Fabien Letouzey

#include <fstream>

#include "hash.h"
#include "files.h"
#include "convert.h"
#include "engine.h"
#include "movegen.h"
#include "book.h"

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

	std::ifstream book_file;
	int book_size;
}

bool book::open()
{
	string path{ files::get_path() };
	path.erase(path.find_last_of("\\") + 1, path.size());
	path += "book.bin";

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
uint16 book::get_move(pos &board)
{
	if (book_file.is_open())
	{
		uint16 move;
		int score, count{ 0 };

		uint64 key{ hashing::to_key(board) };
		book_entry entry;

		for (int i{ find_key(key) }; i < book_size; ++count, ++i)
		{
			read_entry(entry, i);
			if (entry.key != key)
				break;
			move = entry.move;
			score = entry.count;

			assert(score > 0);
			assert(move > 0);
		}
		if (count)
		{
			read_entry(entry, find_key(key) + rand() % count);

			assert(key == entry.key);
			uint16 best_move{ entry.move };

			auto sq1{ 7 - ((best_move & 0x1c0) >> 6) + ((best_move & 0xe00) >> 6) };
			auto sq2{ 7 - (best_move & 0x007) + (best_move & 0x038) };
			auto piece{ board.piece_sq[sq1] };
			auto flag{ board.piece_sq[sq2] };

			if (piece == PAWNS)
			{
				//// enpassant
				if ((~board.side[BOTH] & (1ULL << sq2)) && abs(sq1 - sq2) % 8 != 0)
					flag = ENPASSANT;

				//// promotion
				switch ((best_move & 0x7000) >> 12)
				{
				case 1: flag = PROMO_KNIGHT; break;
				case 2: flag = PROMO_BISHOP; break;
				case 3: flag = PROMO_ROOK; break;
				case 4: flag = PROMO_QUEEN; break;
				default: break;
				}
			}

			//// castling
			else if (piece == KINGS)
			{
				if (sq1 == e1)
				{
					if (sq2 == h1) sq2 = g1, flag = castl_e::SHORT_WHITE;
					else if (sq2 == a1) sq2 = c1, flag = castl_e::LONG_WHITE;
				}
				else if (sq1 == e8)
				{
					if (sq2 == h8) sq2 = g8, flag = castl_e::SHORT_BLACK;
					else if (sq2 == a8) sq2 = c8, flag = castl_e::LONG_BLACK;
				}
			}

			uint16 real_move{ encode(sq1, sq2, flag) };

			//// returning only legal moves
			movegen gen(board, ALL);
			if(gen.in_list(real_move))
				return real_move;
		}
	}

	engine::play_with_book = false;
	return 0;
}

int book::find_key(uint64 &key)
{
	int left{ 0 };
	int right{ book_size - 1 };
	int mid;
	book_entry entry;

	//// binary search (finds the leftmost entry)
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
void book::read_entry(book_entry &entry, int index)
{
	assert(index >= 0 && index < book_size);
	assert(book_file.is_open());

	book_file.seekg(index * 16, std::ios_base::beg);
	assert(book_file.good());

	entry.key = read_int(book_file, 8);
	entry.move = static_cast<uint16>(read_int(book_file, 2));
	entry.count = static_cast<uint16>(read_int(book_file, 2));
	entry.n = static_cast<uint16>(read_int(book_file, 2));
	entry.sum = static_cast<uint16>(read_int(book_file, 2));
}
