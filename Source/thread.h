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


#pragma once

#include <thread>
#include <atomic>
#include <condition_variable>
#include <mutex>

#include "movesort.h"
#include "eval.h"
#include "move.h"
#include "time.h"
#include "board.h"
#include "types.h"
#include "main.h"

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
	std::thread std_thread{};
	std::vector<sthread*> *pool{};
	chronometer chrono{};
	board& pos;

	// every thread has its own search stack

	struct sstack
	{
		depth dt{};
		score sc{ score::none };
		move  mv{};
		move  singular_mv{};
		killer_list killer{};
		struct null_move { bit64 ep{}; square sq{}; } null_mv{};
		move_list quiet_mv{};
		move_list defer_mv{};
		std::array<int, lim::moves> mv_cnt{};
		bool pruning{ true };
	};
	std::array<sstack, lim::dt + 1> stack{}; 

	// other search parameters
	
	history hist{};
	counter_list counter{};
	std::array<key64, 256> rep_hash{};
	bool use_syzygy{};

	// keeping track of the principal variation, node count and tablebase hits

	std::vector<move_var> pv;
	int64 cnt_n{};
	int64 cnt_tbhit{};

	// using king-pawn hash table to speed up the evaluation

	kingpawn_hash hash{ kingpawn_hash::allocate };

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
		if (std_thread.joinable())
			std_thread.join();
	}

	// controlling the search cycles

	void start();
	void start_search();
	void awake();

	// collecting node-count and tablebase-hit information & rearranging PVs

	bool  main() const { return index == 0; }
	int64 get_nodes()  const;
	int64 get_tbhits() const;
	void  rearrange_pv(int mv_cnt);
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

	thread_pool(std::size_t size, board &new_pos) : pos{ new_pos }
	{
		resize(size);
	}

	~thread_pool()
	{
		stop_search();
		resize(0);
	}

	// managing the search threads

	void resize(std::size_t size);
	void start_all() const;
	void stop_search();
	void start_clock(const timemanage::move_time &movetime);
	void extend_time(score drop);
	void clear_history();
	std::tuple<move, move> get_bestmove() const;
};
