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


#include "bitboard.h"
#include "hash.h"

tt::trans* tt::table{ nullptr };
uint64 tt::tt_size{ 0 };

uint64 hashing::to_key(const pos &board)
{
	uint64 key{ 0 };

	// pieces

	for (int col{ WHITE }; col <= BLACK; ++col)
	{
		uint64 pieces{ board.side[col] };
		while (pieces)
		{
			bb::bitscan(pieces);

			auto sq_old{ bb::lsb() };
			auto sq_new{ 7 - (sq_old & 7) + (sq_old & 56) };
			auto piece{ piece_12[board.piece_sq[sq_old]] };

			assert(board.piece_sq[sq_old] != NONE);

			key ^= random64[(piece - col) * 64 + sq_new];
			pieces &= pieces - 1;
		}
	}

	// castling

	for (int i{ 0 }; i < 4; ++i)
	{
		if (board.castl_rights & castl_right[i])
			key ^= random64[offset.castl + i];
	}

	// enpassant

	if (board.ep_square)
	{
		bb::bitscan(board.ep_square);
		auto file_idx{ bb::lsb() & 7 };
		assert((bb::lsb() - file_idx) % 8 == 0);

		if (board.pieces[PAWNS] & board.side[board.turn ^ 1] & ep_flank[board.turn][file_idx])
			key ^= random64[offset.ep + 7 - file_idx];
	}

	// turn

	key ^= is_turn[board.turn];

	return key;
}

int tt::create(uint64 size)
{
	// size is entered in MB

	erase();
	if (size > lim::hash)
		size = lim::hash;
	auto size_temp{ ((size << 20) / sizeof(trans)) >> 1 };

	tt_size = 1ULL;
	for (; tt_size <= size_temp; tt_size <<= 1);

	table = new trans[tt_size];
	size_temp = (tt_size * sizeof(trans)) >> 20;

	assert(size_temp <= size && size_temp <= lim::hash);
	return static_cast<int>(size_temp);
}
void tt::clear()
{
	erase();
	table = new trans[tt_size];
}
void tt::erase()
{
	if(table != nullptr)
	{
		delete[] table;
		table = nullptr;
	}
}

void tt::store(const pos &board, uint16 move, int score, int ply, int depth, uint8 flag)
{
	if (score == score_e::DRAW)
		return;

	trans* entry{ &table[board.key & (tt_size - 1)] };

	if (entry->key == board.key && entry->ply > ply)
		return;

	if (score > score_e::MAX) score += depth;
	if (score < -score_e::MAX) score -= depth;
	assert(abs(score) <= score_e::MATE);

	entry->key = board.key;
	entry->move = move;
	entry->score = score;
	entry->ply = ply;
	entry->flag = flag;
}

bool tt::probe(const pos &board, uint16 &move, int &score, int ply, int depth, uint8 &flag)
{
	trans* entry{ &table[board.key & (tt_size - 1)] };

	if (entry->key == board.key)
	{
		move = entry->move;
		flag = entry->flag;

		if (entry->ply >= ply)
		{
			score = entry->score;

			if (score >  score_e::MAX) score -= depth;
			if (score < -score_e::MAX) score += depth;

			assert(abs(score) <= score_e::MATE);
			return true;
		}
		return false;
	}

	return false;
}
