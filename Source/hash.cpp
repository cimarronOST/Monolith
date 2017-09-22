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


#include "engine.h"
#include "bitboard.h"
#include "hash.h"

namespace
{
	uint32 mirror(uint32 sq)
	{
		return (sq & 56) - (sq & 7) + 7;
	}

	const int order[]{ 0, 2, 1, 3 };
}

uint64 zobrist::to_key(const pos &board)
{
	uint64 key{ 0 };

	// xor all pieces

	for (int col{ WHITE }; col <= BLACK; ++col)
	{
		uint64 pieces{ board.side[col] };
		while (pieces)
		{
			auto sq_old{ bb::bitscan(pieces) };
			auto sq_new{ mirror(sq_old) };
			auto piece{ (board.piece_sq[sq_old] << 1) + (col ^ 1) };

			assert(board.piece_sq[sq_old] != NONE);

			key ^= rand_key[(piece << 6) + sq_new];
			pieces &= pieces - 1;
		}
	}

	// xor castling rights

	for (int i{ 0 }; i < 4; ++i)
	{
		if (board.castl_rights[i])
			key ^= rand_key[offset.castling + order[i]];
	}

	// xor enpassant square

	if (board.ep_sq)
	{
		auto file_idx{ bb::bitscan(board.ep_sq) & 7 };

		if (board.pieces[PAWNS] & board.side[board.turn] & ep_flank[board.not_turn][file_idx])
			key ^= rand_key[offset.ep + 7 - file_idx];
	}

	// xor side to play

	key ^= is_turn[board.turn];

	return key;
}

// transposition table

tt::trans* tt::table{ nullptr };
uint64 tt::tt_size{ 0 };

namespace
{
	uint64 mask;
	const int slots{ 4 };
}

int tt::create(uint64 size)
{
	// building a transposition table of size in MB

	erase();
	if (size > lim::hash)
		size = lim::hash;
	auto size_temp{ ((size << 20) / sizeof(trans)) >> 1 };

	tt_size = 1ULL;
	for (; tt_size <= size_temp; tt_size <<= 1);

	mask = tt_size - slots;

	table = new trans[tt_size];
	clear();
	size_temp = (tt_size * sizeof(trans)) >> 20;

	assert(size_temp <= size && size_temp <= lim::hash);
	return static_cast<int>(size_temp);
}

void tt::reset()
{
	erase();
	table = new trans[tt_size];
	clear();
}

void tt::erase()
{
	if(table != nullptr)
	{
		delete[] table;
		table = nullptr;
	}
}

void tt::clear()
{
	for (uint32 i{ 0 }; i < tt_size; ++i)
		table[i] = { 0ULL, NO_SCORE, NO_MOVE, 0, 0, 0, 0 };
}

int tt::hashfull()
{
	if (tt_size < 1000) return 0;
	int per_mill{ 0 };

	for (int i{ 0 }; i < 1000; ++i)
	{
		if (table[i].key != 0)
			++per_mill;
	}

	return per_mill;
}

void tt::store(const pos &board, uint32 move, int score, int ply, int depth, uint8 flag)
{
	// handling draw- and mate-scores

	if (abs(score) == abs(engine::contempt)) return;

	if (score >  MATE_SCORE) score += depth;
	if (score < -MATE_SCORE) score -= depth;

	assert(abs(score) <= MAX_SCORE);
	assert(ply <= lim::depth);
	assert(ply >= 0);

	// creating a reading pointer

	trans* entry{ &table[board.key & mask] };
	trans* replace{ entry };

	auto lowest{ static_cast<uint32>(lim::depth) + (engine::move_cnt << 8) };

	for (int i{ 0 }; i < slots; ++i, ++entry)
	{
		// always replacing an already existing entry

		if (entry->key == board.key)
		{
			entry->score = static_cast<int16>(score);
			entry->move = static_cast<uint16>(move);
			entry->info = static_cast<uint8>(move >> 16);
			entry->age = static_cast<uint8>(engine::move_cnt);
			entry->ply = static_cast<uint8>(ply);
			entry->flag = flag;
			return;
		}
		
		if (entry->key == 0ULL)
		{
			replace = entry;
			break;
		}

		// looking for the oldest and shallowest entry

		auto new_low{ entry->ply + ((entry->age + (abs(engine::move_cnt - entry->age) & ~0xffU)) << 8) };

		assert(entry->ply <= lim::depth);

		if (new_low < lowest)
		{
			lowest = new_low;
			replace = entry;
		}
	}

	// replacing the oldest and shallowest entry

	replace->key = board.key;
	replace->score = static_cast<int16>(score);
	replace->move = static_cast<uint16>(move);
	replace->info = static_cast<uint8>(move >> 16);
	replace->age = static_cast<uint8>(engine::move_cnt);
	replace->ply = static_cast<uint8>(ply);
	replace->flag = flag;
}

bool tt::probe(const pos &board, uint32 &move, int &score, int ply, int depth, uint8 &flag)
{
	assert(score == NO_SCORE);

	trans* entry{ &table[board.key & mask] };

	for (int i{ 0 }; i < slots; ++i, ++entry)
	{
		if (entry->key == board.key)
		{
			assert(entry->ply <= lim::depth);

			entry->age = static_cast<uint8>(engine::move_cnt);
			move = entry->move | (entry->info << 16);
			flag = entry->flag;

			if (entry->ply >= ply)
			{
				score = entry->score;

				if (score >  MATE_SCORE) score -= depth;
				if (score < -MATE_SCORE) score += depth;

				assert(abs(score) <= MAX_SCORE);
				return true;
			}
			return false;
		}
	}

	return false;
}
