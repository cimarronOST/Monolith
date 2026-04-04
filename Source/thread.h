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


#pragma once

#include <thread>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <vector>
#include <array>
#include <tuple>

#include "history.h"
#include "eval.h"
#include "move.h"
#include "time.h"
#include "board.h"
#include "types.h"

// managing parallelization of the search

class sthread
{
private:
	// variables to synchronize search cycles between the threads

	std::mutex mutex{};
	std::condition_variable cv{};

	bool search{};
	bool exit{};

public:
	// various variables to assure functionality and independence

	int index{};
	std::jthread std_thread{};
	std::vector<sthread*> *pool{};
	chronometer chrono{};
	board& pos;

	// every thread has its own search stack

	std::array<sstack, lim::dt * 2> stack;
	static constexpr int stack_offset{ 5 };
	sstack* stack_front() { return &stack[stack_offset]; }

	// other search parameters
	
	history hist{};
	counter_list counter{};
	std::array<key64, 256> rep_hash{};
	bool use_syzygy{};

	// keeping track of the principal variation, node count, table-base hits and selective depth

	std::vector<move_var> pv;
	int64 cnt_n{};
	int64 cnt_tbhit{};
	int   cnt_root_mv{};
	depth seldt{};

	// using king-pawn hash table to speed up the evaluation

	kingpawn_hash hash{ kingpawn_hash::ALLOCATE };

private:
	void idle();

public:
	// constructor used for the texel tuning method

	sthread(board& new_pos) : pos{ new_pos } { }

	// constructor used for the main search

	sthread(board &new_pos, std::vector<sthread*> *all_threads)
		: pool{ all_threads }, pos{ new_pos } { }

	~sthread()
	{
		exit = true;
		awake();
	}

	// controlling the search cycles

	void start();
	void start_search();
	void awake();
	void init();
	bool stop();
	void check_expiration();
	bool main() const { return index == 0; }

	// retrieving statistics

	int64 get_nodes()  const;
	int64 get_tbhits() const;

	std::tuple<score, bound> probe_syzygy(board& pos, depth dt, depth stack_dt);
	void extend_time(score drop);
	void rearrange_pv();
};

// bundling all the search threads into one interface

class thread_pool
{
private:
	board &pos;

public:
	std::vector<sthread*> thread{};

	// synchronizing thread execution

	static std::mutex mutex;
	static std::condition_variable cv;
	static std::atomic<int> searching;

	thread_pool(std::size_t size, board &new_pos) : pos{ new_pos } { resize(size); }
	~thread_pool() { resize(0); }

	// managing the search threads

	void resize(std::size_t size);
	void start_all() const;
	bool join_main();
	void start_clock(const timemanage::move_time &movetime);
	void clear_history();
	std::tuple<move, move> get_bestmove() const;
};
