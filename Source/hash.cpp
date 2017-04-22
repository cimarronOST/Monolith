/*
  Monolith 0.1  Copyright(C) 2017 Jonas Mayr

  This file is part of Monolith.

  Monolith is free software : you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Monolith is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with Monolith.If not, see <http://www.gnu.org/licenses/>.
*/


#include "bitboard.h"
#include "hash.h"

namespace
{
	struct tt_entry
	{
		uint64 key;
		uint16 move;
		int16 score;
		uint8 ply;
		uint8 flag;
	};

	//// transposition table
	tt_entry* tt{ nullptr };
	uint64 tt_size{ 0 };
}

uint64 hashing::to_key(pos &board)
{
	uint64 key{ 0 };

	//// pieces
	for (int color{ WHITE }; color <= BLACK; ++color)
	{
		uint64 pieces{ board.side[color] };
		while (pieces)
		{
			bb::bitscan(pieces);

			auto sq_old{ bb::lsb() };
			auto sq_new{ 7 - (sq_old & 7) + (sq_old & 56) };
			auto piece{ piece_12[board.piece_sq[sq_old]] };

			assert(board.piece_sq[sq_old] != NONE);

			key ^= random64[(piece - color) * 64 + sq_new];
			pieces &= pieces - 1;
		}
	}

	//// castling
	for (int i{ 0 }; i < 4; ++i)
	{
		if (board.castl_rights & castl_right[i])
			key ^= random64[offset.castl + i];
	}

	//// enpassant
	if (board.ep_square)
	{
		bb::bitscan(board.ep_square);
		auto file{ bb::lsb() & 7 };
		assert((bb::lsb() - file) % 8 == 0);

		if (board.pieces[PAWNS] & board.side[board.turn ^ 1] & ep_flank[board.turn][file])
			key ^= random64[offset.ep + 7 - file];
	}

	//// turn
	key ^= is_turn[board.turn];

	return key;
}

int hashing::tt_create(uint64 size)
{
	//// size is entered in MB

	assert(tt == nullptr);
	if (size > lim::hash)
		size = lim::hash;
	auto size_temp{ ((size << 20) / sizeof(tt_entry)) >> 1 };

	tt_size = 1ULL;
	for (; tt_size <= size_temp; tt_size <<= 1);

	tt = new tt_entry[tt_size];
	size_temp = (tt_size * sizeof(tt_entry)) >> 20;

	assert(size_temp <= size && size_temp <= lim::hash);
	return static_cast<int>(size_temp);
}
void hashing::tt_clear()
{
	tt_delete();
	tt = new tt_entry[tt_size];
}
void hashing::tt_delete()
{
	if(tt != nullptr)
	{
		delete[] tt;
		tt = nullptr;
	}
}

void hashing::tt_save(pos &board, uint16 move, int score, int ply, uint8 flag)
{
	tt_entry* new_entry{ &tt[board.key & (tt_size - 1)] };

	if (new_entry->key == board.key && new_entry->ply > ply) return;

	new_entry->key = board.key;
	new_entry->move = move;
	new_entry->score = score;
	new_entry->ply = ply;
	new_entry->flag = flag;
}

bool hashing::tt_probe(int ply, int alpha, int beta, pos &board, int &score, uint16 &move, uint8 &flag)
{
	tt_entry* entry{ &tt[board.key & (tt_size - 1)] };

	if (entry->key == board.key)
	{
		move = entry->move;
		flag = entry->flag;

		if (entry->ply >= ply)
		{
			score = entry->score;
			return true;
		}
		return false;
	}

	return false;
}