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


#include "attack.h"
#include "zobrist.h"
#include "bit.h"
#include "engine.h"
#include "trans.h"

trans::hash* trans::table{ nullptr };

uint64 trans::hash_hits{ };
uint64 trans::size{ };
uint64 trans::mask{ };

namespace
{
	const int slots{ 4 };
}

namespace
{
	void replace(trans::hash *entry, const board &pos, uint32 move, int score, int bound, int depth)
	{
		// replacing a table entry

		entry->key = pos.key;
		entry->score = score;
		entry->bounds = (bound << 14) | (engine::move_cnt & 0x3fff);
		entry->move = move;
		entry->annex = static_cast<uint8>(move >> 16);
		entry->depth = static_cast<uint8>(depth);
	}
}

int trans::create(uint64 size_mb)
{
	// building a transposition table of <size_mb> megabyte

	assert(size_mb <= lim::hash);

	erase();
	auto size_final{ ((size_mb << 20) / sizeof(hash)) >> 1 };
	size = 1ULL;
	for (; size <= size_final; size <<= 1);
	mask = size - slots;

	table = new hash[size];
	clear();
	size_final = (size * sizeof(hash)) >> 20;

	assert(size_final <= size_mb);
	return static_cast<int>(size_final);
}

void trans::erase()
{
	// erasing the table

	if (table != nullptr)
	{
		delete[] table;
		table = nullptr;
	}
}

void trans::clear()
{
	// resetting the transposition table to zero

	for (auto i{ 0U }; i < size; ++i)
		table[i] = { 0ULL, NO_SCORE, 0U, NO_MOVE, 0U, 0U };
}

uint64 trans::to_key(const board &pos)
{
	// creating a hash key through the board position

	uint64 key{ 0ULL };

	// xor all pieces

	for (int col{ WHITE }; col <= BLACK; ++col)
	{
		auto pieces{ pos.side[col] };
		while (pieces)
		{
			auto sq{ bit::scan(pieces) };
			assert(pos.piece_sq[sq] != NONE);

			key ^= zobrist::rand_key[(pos.piece_sq[sq] * 2 + col) * 64 + sq];
			pieces &= pieces - 1;
		}
	}

	// xor castling rights

	for (auto i{ 0 }; i < 4; ++i)
	{
		if (pos.castling_right[i] != PROHIBITED)
			key ^= zobrist::rand_key[zobrist::off.castling + i];
	}

	// xor enpassant square

	if (pos.ep_sq)
	{
		auto ep_idx{ bit::scan(pos.ep_sq) };

		if (pos.pieces[PAWNS] & pos.side[pos.turn] & attack::king_map[ep_idx] & (bit::rank[R4] | bit::rank[R5]))
			key ^= zobrist::rand_key[zobrist::off.ep + square::file(ep_idx)];
	}

	// xor side to play

	key ^= zobrist::rand_key[zobrist::off.turn] * pos.xturn;

	return key;
}

void trans::store(const board &pos, uint32 move, int score, int bound, int depth, int curr_depth)
{
	// storing a transposition in the table

	assert(abs(score) <= MAX_SCORE);
	assert(bound != 0);
	assert(depth <= lim::depth);
	assert(depth >= 0);
	assert(curr_depth <= lim::depth);
	assert(curr_depth >= 0);

	// adjusting mate scores

	if (bound == EXACT)
	{
		if (score >  MATE_SCORE) score += curr_depth;
		if (score < -MATE_SCORE) score -= curr_depth;
	}

	// probing the table slots

	hash* entry{ &table[pos.key & mask] };
	hash* new_entry{ nullptr };

	auto low_score{ lim::depth + (engine::move_cnt << 8) };

	for (auto i{ 0 }; i < slots; ++i, ++entry)
	{
		// always replacing an already existing entry

		if (entry->key == pos.key || entry->key == 0ULL)
		{
			new_entry = entry;
			break;
		}

		// looking for the oldest and shallowest entry

		auto new_score{ entry->depth + ((entry->bounds & 0x3fff) << 8) };

		assert(entry->depth <= lim::depth);
		assert(new_score  <= lim::depth + (engine::move_cnt << 8));

		if (new_score < low_score)
		{
			low_score = new_score;
			new_entry = entry;
		}
	}

	// replacing the oldest and shallowest entry

	assert (new_entry != nullptr);
	replace(new_entry, pos, move, score, bound, depth);
}

bool trans::probe(const board &pos, uint32 &move, int &score, int &bound, int depth, int curr_depth)
{
	// looking for a hash key match of the current position & retrieving the entry

	move  = NO_MOVE;
	score = NO_SCORE;
	bound = 0;
	hash* entry{ &table[pos.key & mask] };

	for (auto i{ 0 }; i < slots; ++i, ++entry)
	{
		if (entry->key == pos.key)
		{
			hash_hits += 1;

			entry->bounds &= 0xc000;
			entry->bounds |= (engine::move_cnt & 0x3fff);
			move = entry->move | (entry->annex << 16);
			bound = entry->bounds >> 14;

			assert(entry->key != 0ULL);
			assert(entry->depth <= lim::depth);
			assert(bound != 0);
			assert_exp(move == NO_MOVE || pos.pseudolegal(move));

			if (entry->depth >= depth)
			{
				score = entry->score;

				// adjusting mate scores

				if (bound == EXACT)
				{
					if (score >  MATE_SCORE) score -= curr_depth;
					if (score < -MATE_SCORE) score += curr_depth;
				}

				assert(score != NO_SCORE);
				assert(score > MIN_SCORE && score < MAX_SCORE);
				return true;
			}
			return false;
		}
	}
	return false;
}

int trans::hashfull()
{
	// exercising the occupation of the table

	if (size < 1000) return 0;
	auto per_mill{ 0 };

	for (auto i{ 0 }; i < 1000; ++i)
	{
		if (table[i].key != 0ULL)
			per_mill += 1;
	}
	return per_mill;
}