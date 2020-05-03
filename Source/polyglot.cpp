/*
  Monolith 2 Copyright (C) 2017-2020 Jonas Mayr
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


/*
  probing PolyGlot opening books
  all credits go to Fabien Letouzey for creating the book format
  http://hgm.nubati.net/cgi-bin/gitweb.cgi?p=polyglot.git
  the probing code has been modified to conform with the engine
*/

#include "bit.h"
#include "polyglot.h"

namespace move16
{
	// recoding the move coordinates of the book entry

	square sq1(uint16 move16)
	{
		return a1 - ((move16 & 0x1c0) >> 6) + ((move16 & 0xe00) >> 6);
	}

	square sq2(uint16 move16)
	{
		return a1 - (move16 & 0x7) + (move16 & 0x38);
	}

	int promo_pc(uint16 move16)
	{
		return (move16 & 0x7000) >> 12;
	}

	move recode(uint16 move16, const board &pos)
	{
		// recoding move16 to be consistent with the internal board representation

		square sq1{ move16::sq1(move16) };
		square sq2{ move16::sq2(move16) };
		color  cl{ pos.cl };
		piece  pc{ pos.piece_on[sq1] };
		piece  vc{ pos.piece_on[sq2] };
		flag   fl{ no_flag };

		if (pc == pawn)
		{
			// setting en-passant flag

			if (vc == no_piece && std::abs(sq1 - sq2) % 8 != 0)
			{
				fl = enpassant;
				vc = pawn;
			}

			// setting promotion flag

			int promo{ move16::promo_pc(move16) };
			fl = promo ? flag(3 + promo) : fl;
		}

		// setting castling flag

		else if (pc == king && (bit::set(sq2) & pos.side[cl]))
		{
			verify(vc == rook);
			vc = no_piece;
			fl = sq1 > sq2 ? castle_east : castle_west;
		}

		return move{ sq1, sq2, pc, vc, cl, fl };
	}
}

template uint64 book::read_int<uint64>();
template uint16 book::read_int<uint16>();

template<typename uint> uint book::read_int()
{
	// extracting data from the book

	char buf[sizeof(uint)]{};
	stream.read(buf, sizeof(uint));
	verify(stream.good());

	uint assembly{};
	for (auto& b : buf)
		assembly = (assembly << 8) | uint8(b);
	return assembly;
}

void book::read_entry(entry& e, int index)
{
	// retrieving the book entry

	verify(index >= 0 && index < size);
	verify(stream.is_open());
	stream.seekg(index * 16, std::ios_base::beg);
	verify(stream.good());

	e = entry{ read_int<key64>(), read_int<uint16>(), read_int<uint16>(), read_int<uint16>(), read_int<uint16>() };
}

int book::find_key(const key64& key)
{
	// locating the leftmost matching book entry through a binary search

	int left{}, right{ size - 1 }, mid{};
	entry e{};

	verify(left <= right);
	while (left < right)
	{
		mid = (left + right) / 2;
		verify(mid >= left && mid < right);
		read_entry(e, mid);

		if (key <= e.key)
			right = mid;
		else
			left = mid + 1;
	}

	verify(left == right);
	read_entry(e, left);
	return (e.key == key) ? left : size;
}

bool book::open(std::string file)
{
	// opening the book file

	if (stream.is_open())
		stream.close();

	std::string path{ filesystem::path + file };
	stream.open(path, std::ifstream::in | std::ifstream::binary);

	if (!stream.is_open())
		return false;

	stream.seekg(0, std::ios::end);
	size = int(stream.tellg()) / 16;
	if (size == 0)
		return false;

	stream.seekg(0, std::ios::beg);
	hit = stream.good();
	return hit;
}

move book::get_move(const board &pos)
{
	// trying to find a book move

	if (!hit || !stream.is_open() || size == 0)
		return move{};

	int sc{},  best_sc{};
	int cnt{}, best_cnt{};

	key64 key{ polyglot::position_key(pos) };
	entry e{};

	for (int i{ find_key(key) }; i < size; ++cnt, ++i)
	{
		read_entry(e, i);
		if (e.key != key) break;

		// choosing a move randomly, but trending to the best ones

		sc = e.cnt;
		best_sc += sc;
		if (rand_gen.rand32(best_sc) < sc)
			best_cnt = cnt;

		verify(sc > 0 && e.mv != 0);
	}
	if (cnt)
	{
		// recoding, checking & returning the located move

		read_entry(e, find_key(key) + best_cnt);
		verify(key == e.key);

		move bestmove{ move16::recode(e.mv, pos) };
		if (pos.legal(bestmove))
			return bestmove;
	}
	return move{};
}

key64 polyglot::position_key(const board &pos)
{
	// generating the PolyGlot hash key of the current position

	key64 key{};

	// considering all pieces

	for (color cl : { white, black })
	{
		color  cl_x{ cl ^ 1 };
		bit64  pc{ pos.side[cl] };
		while (pc)
		{
			square sq{ bit::scan(pc) };
			verify(pos.piece_on[sq] != no_piece);

			key ^= key_pc[(pos.piece_on[sq] * 2 + cl_x) * 64 + type::sq_flip(sq)];
			pc &= pc - 1;
		}
	}

	// considering castling rights

	for (color cl : {white, black})
		for (flag fl : {castle_east, castle_west})
		{
			if (pos.castle_right[cl][fl] != prohibited)
				key ^= key_castle[cl * 2 + fl];
		}

	// considering en-passant square

	if (pos.ep_rear)
	{
		file fl_ep{ type::fl_of(bit::scan(pos.ep_rear)) };
		if (pos.pieces[pawn] & pos.side[pos.cl] & bit::ep_adjacent[pos.cl_x][fl_ep])
			key ^= key_ep[file_a - fl_ep];
	}

	// considering side to move

	key ^= key_cl * pos.cl_x;

	return key;
}
