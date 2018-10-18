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


#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "move.h"
#include "chronos.h"
#include "pawn.h"
#include "position.h"
#include "main.h"

// managing parallel threading of the search

class sthread
{
	// variables to synchronize search cycles

private:
	std::mutex mutex;
	std::condition_variable cv;

	bool search{};
	bool exit{};

	// various variables to assure functionality and independence

public:
	std::thread std_thread;
	std::vector<sthread*> *pool;
	board &pos;
	chronometer chrono;
	bool main{};

	// uncluttering search parameters with the search-stack

	struct sstack
	{
		int depth;
		uint32 move;
		uint32 skip_move;
		uint32 killer[2];
		struct null_copy
		{
			uint64 ep;
			uint16 capture;
		} copy;
		bool no_pruning;
	} stack[lim::depth + 1]{};

	// search & move-ordering parameters

	int history[2][6][64]{};
	uint32 counter_move[6][64]{};
	uint64 rep_hash[256 + lim::depth]{};
	bool use_syzygy{};

	// keeping track of the principal variation

	std::vector<move::variation> pv;
	uint32 tri_pv[lim::depth + 1][lim::depth]{};

	// counting nodes

	int64 nodes{};
	struct node_count
	{
		int64 tbhit;
		int64 fail_high;
		int64 fail_high_1;
		int64 qs;
	} count{};

	// using pawn hash table

	pawn pawnhash;

private:
	void idle_loop();

public:
	sthread(board &new_pos, std::vector<sthread*> *all_threads)
		: pool(all_threads), pos(new_pos), pawnhash(true) { }

	~sthread()
	{
		exit = true;
		awake();
		if (std_thread.joinable())
			std_thread.join();
	}

	// controlling the search cycles

	void start();
	void start_search();
	void awake();

	// collecting node-count information

	int64 get_nodes()  const;
	int64 get_tbhits() const;
};

// bundling all the search threads into one interface

class thread_pool
{
private:
	board &pos;

	// containing the search threads

public:
	std::vector<sthread*> thread;

	// synchronizing thread execution

	static std::mutex mutex;
	static std::condition_variable cv;
	static std::atomic<int> searching;

	thread_pool(uint32 size, board &new_pos) : pos(new_pos)
	{
		resize(size);
	}

	~thread_pool()
	{
		stop_search();
		resize(0);
	}

	// managing the search threads

	void resize(uint32 size);
	void start_all() const;
	void stop_search();
	void start_clock(int64 &movetime);
};