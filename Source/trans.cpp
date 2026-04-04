/*
  Monolith Copyright (C) 2017-2026 Jonas Mayr

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


#include <cstring>
#include <memory>
#include <iostream>
#include <vector>
#include <thread>
#include <tuple>
#include <algorithm>

#include "main.h"
#include "types.h"
#include "move.h"
#include "uci.h"
#include "trans.h"

#if defined(__linux__)
#include <sys/mman.h>
#endif

std::unique_ptr<trans::hash[], trans::std_free> trans::table{};
uint64 trans::size{};
uint64 trans::mask{};

void trans::hash::update_key(const key64& new_key)
{
	key = new_key ^ data;
}

void trans::hash::update_age(int new_age)
{
	data &= 0xffffffffffff8000ULL;
	data |= new_age & 0x7fff;
}

namespace compress
{
	// compressing the data to fit into the 8 data bytes of a table entry

	static uint64 data(score sc, move mv, depth dt, bound bd, int age)
	{
		verify(type::sc(sc));
		verify((mv.raw() >> 24) == 0U);
		verify(type::dt(dt));
		verify(bd == bound::EXACT || bd == bound::UPPER || bd == bound::LOWER);
		verify(age >= 0);
        return (uint64(sc - score::NONE) << 48)
			 | (uint64(mv.raw()) << 24)
			 | (uint64(dt) << 17)
			 | (uint64(bd) << 15)
			 | (uint64(age) & 0x7fff);
	}
}

namespace get
{
	// decompressing the 8 data bytes of a table entry

	static score sc(uint64& data)
	{
		score sc{ score(data >> 48) + score::NONE };
		verify(type::sc(sc));
		return sc;
	}

	static move mv(uint64& data)
	{
		return move{ uint32((data >> 24) & 0xffffffU) };
	}

	static depth dt(uint64& data)
	{
		depth dt{ depth(data >> 17) & 0x7f };
		verify(type::dt(dt));
		return dt;
	}

	static bound bd(uint64& data)
	{
		bound bd{ bound((data >> 15) & 0x3) };
		verify(bd == bound::EXACT || bd == bound::UPPER || bd == bound::LOWER);
		return bd;
	}

	static int age(uint64& data)
	{
		return int(data & 0x7fff);
	}
}

std::size_t trans::create(std::size_t megabytes)
{
	// building a transposition table
	// using a vector would be more elegant for this, but then aligned allocation wouldn't be possible

	verify(megabytes <= lim::hash);
	std::size_t bytes{ megabytes << 20 };
	std::size_t entries_max{ bytes / sizeof(hash) };

	// begin with the smallest possible hash size of 2 megabytes
	// 2^17 entries of 16 bytes each are needed to complete 2 megabytes
	// then compute how many entries fit into the given megabytes

	std::size_t entries{ 1ULL << 17 };
	static_assert((1ULL << 17) * sizeof(hash) == 2 * (1ULL << 20));
	for (; entries <= entries_max / 2; entries <<= 1);

	// aligning on 2 megabyte boundaries and requesting Huge Pages on Linux systems
	// the Android NDK doesn't seem to support aligned_alloc

#if defined(__linux__) && !defined(__ANDROID__)
	table.reset((hash*)aligned_alloc(2 * (1ULL << 20), entries * sizeof(hash)));
	madvise(table.get(), entries, MADV_HUGEPAGE);

#else
	table.reset((hash*)malloc(entries * sizeof(hash)));
#endif

	if (!table)
	{
		std::cout << "info string warning: memory allocation for main hash table failed" << std::endl;
		return 0;
	}

	size = entries;
	mask = entries - slots;
	verify(hashfull() == 0ULL);
	return (entries * sizeof(hash)) >> 20;
}

void trans::clear()
{
	// clearing the hash table

	auto chunk{ size / uci::thread_cnt };
	std::vector<std::thread> threads;

	for (int idx{}; idx < uci::thread_cnt; ++idx)
		threads.emplace_back(&trans::clear_fast, this, idx, chunk);
	for (auto& t : threads)
		t.join();
}

void trans::clear_fast(int idx, uint64 chunk)
{
	auto append{ idx == uci::thread_cnt - 1 ? size - chunk * idx : chunk };
	verify(idx * chunk + append <= size);
	std::memset(&table[idx * (size_t)chunk], 0, (size_t)append * sizeof(hash));
}

trans::hash* trans::new_entry(const key64& key, move& new_mv, bound bd)
{
	// searching for the most suitable slot for storage

	hash* entry{ get_entry(key) };
	hash* new_entry{ entry };
	score max_sc{ score(lim::dt + (uci::mv_cnt << 8)) };
	score low_sc{ max_sc };

	for (int i{}; i < slots; ++i, ++entry)
	{
		if (entry->key == 0ULL)
			return entry;

		// always replacing an already existing entry
		// for fail-low nodes after a previous fail high, that move that failed high is stored (? Elo)

		if ((entry->key ^ entry->data) == key)
		{
			if (bd == bound::UPPER && get::bd(entry->data) == bound::LOWER)
				new_mv = get::mv(entry->data);
			return entry;
		}

		// otherwise looking for the oldest and shallowest entry

		score new_sc{ score(get::dt(entry->data) + (get::age(entry->data) << 8)) };
		if (new_sc <= low_sc)
		{
			low_sc = new_sc;
			new_entry = entry;
		}

		// always replacing entries "from the future" in case of moving backwards through a game while analyzing
		// and not clearing the hash-table between searches

		else if (new_sc > max_sc)
			return entry;
	}
	return new_entry;
}

void trans::store(const key64& key, move mv, std::tuple<score, bound> bounded_sc, depth remaining_dt, depth curr_dt)
{
	// storing a transposition in the hash table

	score sc{ std::get<0>(bounded_sc) };
	bound bd{ std::get<1>(bounded_sc) };

	verify(type::sc(sc));
	verify(type::dt(remaining_dt));
	verify(type::dt(curr_dt));
	verify(bd == bound::EXACT || bd == bound::UPPER || bd == bound::LOWER);
	verify(size - slots == mask);
	verify((key & mask) < size);

	// adjusting mate scores

	if (sc >  LONGEST_MATE && bd != bound::UPPER) sc += curr_dt;
	if (sc < -LONGEST_MATE && bd != bound::LOWER) sc -= curr_dt;

	// probing the table for the best match out of the 4 possible slots

	move  new_mv{ mv };
	hash* entry{ new_entry(key, new_mv, bd) };

	// storing the new entry

	entry->data = compress::data(sc, new_mv, remaining_dt, bd, uci::mv_cnt);
	entry->update_key(key);
}

trans::hash* trans::get_entry(const key64& key)
{
	// getting the table entry indexed by the hash key of the position

	verify(size - slots == mask);
	verify((key & mask) < size);

	return &table[std::size_t(key & mask)];
}

bool trans::entry::probe(const key64& key, depth curr_dt)
{
	// looking for a hash key match of the current position & retrieving the entry

	verify(type::dt(curr_dt));

	*this = {};
	hash* entry{ get_entry(key) };

	for (int i{}; i < slots; ++i, ++entry)
	{
		if ((entry->key ^ entry->data) == key)
		{
			entry->update_age(uci::mv_cnt);
			entry->update_key(key);

			mv = get::mv(entry->data);
			sc = get::sc(entry->data);
			bd = get::bd(entry->data);
			dt = get::dt(entry->data);

			// adjusting mate scores

			if (sc >  LONGEST_MATE && bd != bound::UPPER) sc -= curr_dt;
			if (sc < -LONGEST_MATE && bd != bound::LOWER) sc += curr_dt;

			verify(entry->key);
			verify(type::sc(sc));
			return true;
		}
	}
	return false;
}

int trans::hashfull()
{
	// determining the occupation of the table

	verify(size >= 2 * (1ULL << 20) / sizeof(hash));
	verify(size >= 1000);

	return (int)std::count_if(table.get(), table.get() + 1000, [&](hash& entry) { return entry.key != 0ULL; });
}