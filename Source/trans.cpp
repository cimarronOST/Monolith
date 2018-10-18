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


#include "utilities.h"
#include "attack.h"
#include "zobrist.h"
#include "bit.h"
#include "uci.h"
#include "trans.h"

struct trans::hash* trans::table{};

uint64 trans::hash_hits{};
uint64 trans::size{};
uint64 trans::mask{};

namespace get
{
	// encoding & decoding the compressed data of the 8-byte hash-entry

	uint64 data(int score, uint32 move, int depth, int bound, int age)
	{
		assert(score > -SCORE_MATE && score < SCORE_MATE);
		assert(move  >> 24 == 0U);
		assert(depth >= 0 && depth <= lim::depth);
		assert(bound == EXACT || bound == UPPER || bound == LOWER);
		assert(age   >= 0);
		return (static_cast<uint64>(score - SCORE_NONE) << 48)
			 | (static_cast<uint64>(move)  << 24)
			 | (static_cast<uint64>(depth) << 17)
			 | (static_cast<uint64>(bound) << 15)
			 | (static_cast<uint64>(age) & 0x7fff);
	}

	int score(uint64 &data)
	{
		int score{ static_cast<int>(data >> 48) + SCORE_NONE };
		assert(score > -SCORE_MATE && score < SCORE_MATE);
		return score;
	}

	uint32 move(uint64 &data)
	{
		return static_cast<uint32>(data >> 24) & 0xffffffU;
	}

	int depth(uint64 &data)
	{
		int depth{ static_cast<int>(data >> 17) & 0x7f };
		assert(depth <= lim::depth);
		return depth;
	}

	int bound(uint64 &data)
	{
		int bound{ static_cast<int>(data >> 15) & 0x3 };
		assert(bound == EXACT || bound == UPPER || bound == LOWER);
		return bound;
	}

	int age(uint64 &data)
	{
		return static_cast<int>(data & 0x7fff);
	}
}

namespace update
{
	void age(uint64 &data, int new_age)
	{
		// updating the entry's age at every hash hit

		data &= 0xffffffffffff8000;
		data |= new_age & 0x7fff;
	}

	void key(uint64 &key, uint64 &data, const uint64 &pos_key)
	{
		// updating the key whenever data changes

		key = pos_key ^ data;
	}
}

int trans::create(uint64 size_mb)
{
	// building a transposition table of <size_mb> megabytes

	assert(size_mb <= lim::hash);
	auto boundary{ (size_mb << 20) / sizeof(hash) / 2 };
	for (size = 1ULL; size <= boundary; size <<= 1);

	erase();
	table = new hash[size];
	clear();
	mask = size - slots;

	auto final_size_mb{ (size * sizeof(hash)) >> 20 };
	assert(final_size_mb <= size_mb);
	return static_cast<int>(final_size_mb);
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

	for (uint32 i{}; i < size; ++i)
		table[i] = hash{};
}

uint64 trans::to_key(const board &pos)
{
	// creating a hash key of the board position

	uint64 key{};

	// considering all pieces

	for (int color{ WHITE }; color <= BLACK; ++color)
	{
		auto pieces{ pos.side[color] };
		while (pieces)
		{
			auto sq{ bit::scan(pieces) };
			assert(pos.piece[sq] != NONE);

			key ^= zobrist::rand_key[(pos.piece[sq] * 2 + color) * 64 + sq];
			pieces &= pieces - 1;
		}
	}

	// considering castling rights

	for (int i{}; i < 4; ++i)
	{
		if (pos.castling_right[i] != PROHIBITED)
			key ^= zobrist::rand_key[zobrist::off.castling + i];
	}

	// considering en-passant square only if a capturing pawn stands ready

	if (pos.ep_rear)
	{
		assert(bit::shift(pos.ep_rear, shift::push[pos.xturn]) & pos.pieces[PAWNS] & pos.side[pos.xturn]);

		auto ep_index{ bit::scan(pos.ep_rear) };
		if (pos.pieces[PAWNS] & pos.side[pos.turn] & attack::king_map[ep_index] & (bit::rank[R4] | bit::rank[R5]))
			key ^= zobrist::rand_key[zobrist::off.ep + index::file(ep_index)];
	}

	// considering side to move

	key ^= zobrist::rand_key[zobrist::off.turn] * pos.xturn;

	return key;
}

void trans::store(uint64 &key, uint32 move, int score, int bound, int depth, int curr_depth)
{
	// storing a transposition in the hash table

	assert(std::abs(score) <= SCORE_MATE);
	assert(0 <= curr_depth && curr_depth <= lim::depth);

	// adjusting mate scores

	if (bound == EXACT)
	{
		if (score >  SCORE_LONGEST_MATE) score += curr_depth;
		if (score < -SCORE_LONGEST_MATE) score -= curr_depth;
	}

	// probing the table slots

	hash* entry{ &table[key & mask] };
	hash* new_entry{};

	auto max_score{ lim::depth + (uci::move_count << 8) };
	auto low_score{ max_score };

	for (int i{}; i < slots; ++i, ++entry)
	{
		// always replacing an already existing entry

		if ((entry->key ^ entry->data) == key || entry->key == 0ULL)
		{
			new_entry = entry;
			break;
		}

		// looking for the oldest and shallowest entry

		auto new_score{ get::depth(entry->data) + (get::age(entry->data) << 8) };
		if  (new_score <= low_score)
		{
			low_score = new_score;
			new_entry = entry;
		}

		// always replacing "entries from the future" in case of moving backwards through a game while analyzing
		// and the hash has not been cleared

		else if (new_score > max_score)
		{
			new_entry = entry;
			break;
		}
	}

	// replacing the oldest and shallowest entry

	assert(new_entry);
	new_entry->data = get::data(score, move, depth, bound, uci::move_count);
	update::key(new_entry->key, new_entry->data, key);
}

bool trans::probe(uint64 &key, entry &node, int depth, int curr_depth)
{
	// looking for a hash key match of the current position & retrieving the entry

	node = { MOVE_NONE, SCORE_NONE, 0, 0 };
	hash* entry{ &table[key & mask] };

	for (int i{}; i < slots; ++i, ++entry)
	{
		if ((entry->key ^ entry->data) == key)
		{
			hash_hits += 1;
			update::age(entry->data, uci::move_count);
			update::key(entry->key, entry->data, key);
			
			node.move  = get::move(entry->data);
			node.bound = get::bound(entry->data);
			node.depth = get::depth(entry->data);

			assert(entry->key);

			if (node.depth >= depth)
			{
				node.score = get::score(entry->data);

				// adjusting mate scores

				if (node.bound == EXACT)
				{
					if (node.score >  SCORE_LONGEST_MATE) node.score -= curr_depth;
					if (node.score < -SCORE_LONGEST_MATE) node.score += curr_depth;
				}

				assert(-SCORE_MATE < node.score && node.score < SCORE_MATE);
				return true;
			}
			return false;
		}
	}
	return false;
}

int trans::hashfull()
{
	// determining the occupation of the table

	assert(size >= (1ULL << 20) / sizeof(hash) / 2 && size >= 1000);
	return static_cast<int>(std::count_if(table, table + 1000,
		[&](hash &entry) { return entry.key != 0ULL; }));
}