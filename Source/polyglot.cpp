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


// this file is based on PolyGlot by Fabien Letouzey:
// http://hgm.nubati.net/cgi-bin/gitweb.cgi?p=polyglot.git

#include <fstream>

#include "utilities.h"
#include "move.h"
#include "bit.h"
#include "attack.h"
#include "zobrist.h"
#include "stream.h"
#include "polyglot.h"

std::string book::name{ "monolith.bin" };

namespace
{
	static_assert(PROMO_KNIGHT == 12, "promotion flag");
	static_assert(PROMO_QUEEN  == 15, "promotion flag");

	constexpr int castling_flag[]{ CASTLE_SHORT, CASTLE_LONG };

	std::ifstream stream;
	int book_size{};
	rand_32 rand_gen;
}

namespace move
{
	bool legal(board pos, uint32 move)
	{
		// checking for full legality of the book move

		if (!pos.pseudolegal(move))
			return false;

		pos.new_move(move);
		return attack::check(pos, pos.xturn, pos.pieces[KINGS] & pos.side[pos.xturn]);
	}
}

namespace move16
{
	// decoding the move coordinates of the book entry

	int sq1(uint16 move16)
	{
		return 7 - ((move16 & 0x1c0) >> 6) + ((move16 & 0xe00) >> 6);
	}

	int sq2(uint16 move16)
	{
		return 7 - (move16 & 0x7) + (move16 & 0x38);
	}

	int promo(uint16 move16)
	{
		return (move16 & 0x7000) >> 12;
	}

	uint32 recode(uint16 move16, const board &pos)
	{
		// recoding <move16> to be consistent with the internal board representation

		auto sq1{ move16::sq1(move16) };
		auto sq2{ move16::sq2(move16) };
		auto turn{ pos.turn };

		int flag{ NONE };
		int piece{ pos.piece[sq1] };
		int victim{ pos.piece[sq2] };

		if (piece == PAWNS)
		{
			// setting en-passant flag

			if (victim == NONE && std::abs(sq1 - sq2) % 8 != 0)
			{
				flag = ENPASSANT;
				victim = PAWNS;
			}

			// setting double-push flag

			if (std::abs(sq1 - sq2) == 16)
				flag = DOUBLEPUSH;

			// setting promotion flag

			auto promo{ move16::promo(move16) };
			flag = { promo ? 11 + promo : flag };
		}

		// setting castling flag

		else if (piece == KINGS && ((1ULL << sq2) & pos.side[turn]))
		{
			assert(victim == ROOKS);
			victim = NONE;
			flag = castling_flag[sq1 > sq2];
		}

		return move::encode(sq1, sq2, flag, piece, victim, turn);
	}
}

namespace data
{
	template<typename uint>
	uint read_int()
	{
		// extracting data from the book

		char buf[sizeof(uint)];
		stream.read(buf, sizeof(uint));
		assert(stream.good());

		uint assembly{};
		for (auto &b : buf)
			assembly = (assembly << 8) | static_cast<uint8>(b);
		return assembly;
	}
}

namespace book
{
	struct book_entry
	{
		uint64 key;
		uint16 move;
		uint16 count;
		uint16 n;
		uint16 sum;
	};

	void read_entry(book_entry &entry, int index)
	{
		// retrieving the book entry

		assert(index >= 0 && index < book_size);
		assert(stream.is_open());

		stream.seekg(index * 16, std::ios_base::beg);
		assert(stream.good());

		entry.key = data::read_int<uint64>();
		entry.move = data::read_int<uint16>();
		entry.count = data::read_int<uint16>();
		entry.n = data::read_int<uint16>();
		entry.sum = data::read_int<uint16>();
	}

	int find_key(uint64 &key)
	{
		// locating the leftmost matching book entry through a binary search

		int left{};
		int right{ book_size - 1 };
		int mid{};
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
}

bool book::open()
{
	// opening the opening book file

	if (stream.is_open())
		stream.close();

	std::string path{ filestream::fullpath + name };

	stream.open(path, std::ifstream::in | std::ifstream::binary);
	if (!stream.is_open())
		return false;

	stream.seekg(0, std::ios::end);
	book_size = static_cast<int>(stream.tellg()) / 16;
	if (book_size == 0)
		return false;

	stream.seekg(0, std::ios::beg);
	if (!stream.good())
		return false;

	return true;
}

uint32 book::get_move(board &pos)
{
	// trying to find a book move

	if (stream.is_open() && book_size != 0)
	{
		int score{}, best_score{};
		int count{}, best_count{};

		uint64 key{ polyglot::to_key(pos) };
		book_entry entry{};

		for (auto i{ find_key(key) }; i < book_size; ++count, ++i)
		{
			read_entry(entry, i);
			if (entry.key != key) break;

			// choosing a move randomly, but trending to the best ones

			score = entry.count;
			best_score += score;
			if (rand_gen.rand32(best_score) < score)
				best_count = count;
			assert(score > 0 && entry.move != 0);
		}

		if (count)
		{
			// recoding & checking & returning the located move

			read_entry(entry, find_key(key) + best_count);
			assert(key == entry.key);

			auto best_move{ move16::recode(entry.move, pos) };
			if (move::legal(pos, best_move))
				return best_move;
		}
	}

	// no legal move has been found

	return MOVE_NONE;
}

namespace
{
	constexpr int castling_idx[]{ 0, 2, 1, 3 };
}

uint64 polyglot::to_key(const board &pos)
{
	// generating the PolyGlot hash key of the current position

	uint64 key{};

	// considering all pieces

	for (int col{ WHITE }; col <= BLACK; ++col)
	{
		auto xcol{ col ^ 1 };
		uint64 pieces{ pos.side[col] };
		while (pieces)
		{
			auto sq{ bit::scan(pieces) };
			assert(pos.piece[sq] != NONE);

			key ^= polyglot::rand_key[(pos.piece[sq] * 2 + xcol) * 64 + (sq & 56) + 7 - index::file(sq)];
			pieces &= pieces - 1;
		}
	}

	// considering castling rights

	for (auto i{ 0 }; i < 4; ++i)
	{
		if (pos.castling_right[i] != PROHIBITED)
			key ^= polyglot::rand_key[zobrist::off.castling + castling_idx[i]];
	}

	// considering en-passant square

	if (pos.ep_rear)
	{
		auto ep_idx{ bit::scan(pos.ep_rear) };
		if (pos.pieces[PAWNS] & pos.side[pos.turn] & attack::king_map[ep_idx] & (bit::rank[R4] | bit::rank[R5]))
			key ^= polyglot::rand_key[zobrist::off.ep + 7 - index::file(ep_idx)];
	}

	// considering side to move

	key ^= polyglot::rand_key[zobrist::off.turn] * pos.xturn;

	return key;
}