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


#include <tuple>
#include <bit>
#include <algorithm>
#include <mutex>
#include <condition_variable>

#include "main.h"
#include "types.h"
#include "syzygy.h"
#include "uci.h"
#include "move.h"
#include "board.h"
#include "thread.h"

std::mutex thread_pool::mutex{};
std::condition_variable thread_pool::cv{};
std::atomic<int> thread_pool::searching{};

void sthread::idle()
{
	// all search threads are created only once and then switch between searching and waiting

	while (true)
	{
		std::unique_lock<std::mutex> lock(mutex);
		cv.wait(lock, [&] { return search; });
		if (exit) break;

		start_search();
		search = false;
	}
}

void sthread::start()
{
	// detaching the thread from the main thread and capturing it inside the idle-loop

	verify(!main() && !search && !exit);
	std_thread = std::jthread{ &sthread::idle, this };
}

void sthread::awake()
{
	// awaking the thread from its idle-loop to let it start searching

	search = true;
	cv.notify_one();
}

void sthread::init()
{
	// initializing all parameters at the beginning of each search

	verify(uci::mv_offset < (int)rep_hash.size());
	verify(uci::mv_offset < (int)uci::game_hash.size());

	cnt_n = cnt_tbhit = cnt_root_mv = seldt = 0;

	for (int i{}; i <= uci::mv_offset; ++i)
		rep_hash[i] = uci::game_hash[i];

	for (depth dt{}; dt < (depth)stack.size(); ++dt)
	{
		stack[dt] = sstack{};
		stack[dt].dt = std::max(0, dt - stack_offset);
	}

	for (auto& cl : counter)
		for (auto& pc : cl)
			for (auto& sq : pc)
				sq = move{};

	pv.clear();
	pv.resize(uci::multipv);
}

bool sthread::stop()
{
	// keeping track of the elapsing time
	// also increasing the frequency of checking the elapsed time while probing Syzygy table-bases

	if (++chrono.hits < chrono.hit_threshold)
		return false;
	chrono.hits = 0;
	if (uci::infinite.load(std::memory_order::relaxed))
		return false;
	if (get_nodes() >= uci::limit.nodes)
		return true;

	return chrono.elapsed() >= chrono.movetime.target;
}

void sthread::check_expiration()
{
	// checking for immediate search termination
	// to always guarantee correct output, at least depth 1 must have been completed

	if ((uci::stop.load(std::memory_order::relaxed) || stop()) && pv[0].dt > 1)
	{
		uci::stop = true;
		throw exception::STOP_SEARCH;
	}
}

int64 sthread::get_nodes() const
{
	// summing up the node count of all threads

	int64 nodes{};
	for (auto& t : *pool) nodes += t->cnt_n;
	return nodes;
}

int64 sthread::get_tbhits() const
{
	// summing up the table-base-hit count of all threads

	int64 hits{};
	for (auto& t : *pool) hits += t->cnt_tbhit;
	return hits;
}

std::tuple<score, bound> sthread::probe_syzygy(board& board_pos, depth dt, depth stack_dt)
{
	// probing Syzygy endgame table-bases 

    int cnt{ std::popcount(board_pos.side[BOTH]) };

	if (use_syzygy
		&& board_pos.half_cnt == 0
		&& cnt <= lim::syzygy_pieces
		&& (dt >= uci::syzygy_dt || cnt < std::min(5, lim::syzygy_pieces)))
	{
		chrono.hits = chrono.hit_threshold;
		int success{};
		if (int wdl{ syzygy::probe_wdl(board_pos, success) }; success)
		{
			// converting the table-base WinDrawLoss score into a more convenient score and bound

			verify(wdl >= -2 && wdl <= 2);
			cnt_tbhit += 1;
			switch (wdl)
			{
            case  2: return { TB_WIN  - score(stack_dt), bound::LOWER };
            case -2: return { TB_LOSS + score(stack_dt), bound::UPPER };
            default: return { DRAW    + score(wdl),      bound::EXACT };
			}
		}
	}

    return { score::NONE, bound::NONE };
}

void sthread::extend_time(score drop)
{
	// extending the search-time if the score is dropping

	for (auto& t : *pool) t->chrono.extend(drop);
}

void sthread::rearrange_pv()
{
	// sorting multiple principal variations

	verify(pv.size() == uci::multipv);
	std::stable_sort(pv.begin(), std::min(pv.begin() + cnt_root_mv, pv.end()),
		[&](move_var a, move_var b) { return a.sc > b.sc; });
}

void thread_pool::resize(std::size_t size)
{
	// adjusting the number of search-threads

	verify(size <= lim::threads);
	while (thread.size() > 0)
	{
		delete thread.back();
		thread.pop_back();
	}

	while (thread.size() < size)
	{
		thread.push_back(new sthread(pos, &this->thread));
		thread.back()->index = (int)thread.size() - 1;
	}
}

void thread_pool::start_all() const
{
	// starting all search-threads except the main search-thread

	for (uint32 i{ 0 + 1 }; i < thread.size(); ++i)
		thread[i]->start();
}

bool thread_pool::join_main()
{
	// quitting the main thread after the UCI 'stop' command

	if (!uci::stop)
		return false;
	else
	{
		if (thread[0]->std_thread.joinable())
			thread[0]->std_thread.join();
		return true;
	}
}

void thread_pool::start_clock(const timemanage::move_time &movetime)
{
	// starting the clock in each thread

	for (auto t : thread) t->chrono.set(movetime);
}

void thread_pool::clear_history()
{
	// clearing the history tables only between new games but not between searches

	for (auto t : thread)
		t->hist.clear();
}

std::tuple<move, move> thread_pool::get_bestmove() const
{
	// looking for the best move of all the search threads at the end of the search
	// the best move of the thread that finished the highest iteration seems the most promising
	// the second move returned is the ponder move
	
	int idx{};
	depth dt{};
	for (auto& t : thread)
	{
		if (t->pv[0].dt > dt && t->pv[0].mv[0] && t->pv[0].pos_key == pos.key.pos)
		{
			dt  = t->pv[0].dt;
			idx = t->index;
		}
	}
	move bestmv{ thread[idx]->pv[0].mv[0] };
	move ponder{ thread[idx]->pv[0].cnt > 1 ? thread[idx]->pv[0].mv[1] : move{} };

	// printing the search details corresponding to the best move

	if (!thread[idx]->main())
		uci::info_iteration(*thread[idx]);
	return std::make_tuple(bestmv, ponder);
}