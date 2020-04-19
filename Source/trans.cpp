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


#include <cstdlib>
#include <cstring>

#include "uci.h"
#include "trans.h"

#if defined(__linux__)
#include <sys/mman.h>
#endif

std::unique_ptr<trans::hash[], trans::std_free> trans::table{};
uint64 trans::size{};
uint64 trans::mask{};

namespace compress
{
	// compressing the data to fit into the 8 data bytes of a table entry

	uint64 data(score sc, move mv, depth dt, bound bd, int age)
	{
		verify(type::sc(sc));
		verify((mv.raw() >> 24) == 0U);
		verify(type::dt(dt));
		verify(bd == bound::exact || bd == bound::upper || bd == bound::lower);
		verify(age >= 0);
		return (uint64(sc - score::none) << 48)
			| (uint64(mv.raw()) << 24)
			| (uint64(dt) << 17)
			| (uint64(bd) << 15)
			| (uint64(age) & 0x7fff);
	}
}

namespace get
{
	// decompressing the 8 data bytes of a table entry

	score sc(uint64& data)
	{
		score sc{ score(data >> 48) + score::none };
		verify(type::sc(sc));
		return sc;
	}

	move mv(uint64& data)
	{
		return move{ uint32((data >> 24) & 0xffffffU) };
	}

	depth dt(uint64& data)
	{
		depth dt{ depth(data >> 17) & 0x7f };
		verify(type::dt(dt));
		return dt;
	}

	bound bd(uint64& data)
	{
		bound bd{ bound((data >> 15) & 0x3) };
		verify(bd == bound::exact || bd == bound::upper || bd == bound::lower);
		return bd;
	}

	int age(uint64& data)
	{
		return int(data & 0x7fff);
	}
}

namespace update
{
	void age(uint64& data, int new_age)
	{
		// updating the entry's age at every hash hit

		data &= 0xffffffffffff8000ULL;
		data |= new_age & 0x7fff;
	}

	void key(key64& key, const uint64& data, const key64& key_pos)
	{
		// updating the key whenever an entry's data gets updated

		key = key_pos ^ data;
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
	// since the speed of this call is crucial for dealing with a large hash table, memset is called

	std::memset(&table[0], 0, (std::size_t)size * sizeof(hash));
}

void trans::store(const key64& key, move mv, score sc, bound bd, depth remaining_dt, depth curr_dt)
{
	// storing a transposition in the hash table

	verify(type::sc(sc));
	verify(type::dt(remaining_dt));
	verify(type::dt(curr_dt));
	verify(bd == bound::exact || bd == bound::upper || bd == bound::lower);
	verify(size - slots == mask);
	verify((key & mask) < size);

	// adjusting mate scores

	if (sc >  score::longest_mate && bd != bound::upper) sc += curr_dt;
	if (sc < -score::longest_mate && bd != bound::lower) sc -= curr_dt;

	// probing the table slots

	hash* entry{ get_entry(key) };
	hash* new_entry{ entry };

	score max_sc{ score(lim::dt + (uci::mv_cnt << 8)) };
	score low_sc{ max_sc };

	for (int i{}; i < slots; ++i, ++entry)
	{
		// always replacing an already existing entry

		if ((entry->key ^ entry->data) == key || entry->key == 0ULL)
		{
			new_entry = entry;
			break;
		}

		// otherwise looking for the oldest and shallowest entry

		score new_sc{ score(get::dt(entry->data) + (get::age(entry->data) << 8)) };
		if (new_sc <= low_sc)
		{
			low_sc = new_sc;
			new_entry = entry;
		}

		// always replacing entries "from the future" in case of moving backwards through a game while analyzing
		// and the hash is not cleared before every search

		else if (new_sc > max_sc)
		{
			new_entry = entry;
			break;
		}
	}

	// replacing the oldest and shallowest entry

	new_entry->data = compress::data(sc, mv, remaining_dt, bd, uci::mv_cnt);
	update::key(new_entry->key, new_entry->data, key);
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
			update::age(entry->data, uci::mv_cnt);
			update::key(entry->key, entry->data, key);

			mv = get::mv(entry->data);
			sc = get::sc(entry->data);
			bd = get::bd(entry->data);
			dt = get::dt(entry->data);

			// adjusting mate scores

			if (sc >  score::longest_mate && bd != bound::upper) sc -= curr_dt;
			if (sc < -score::longest_mate && bd != bound::lower) sc += curr_dt;

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