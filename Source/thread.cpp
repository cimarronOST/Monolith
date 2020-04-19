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


#include "uci.h"
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
	std_thread = std::thread{ &sthread::idle, this };
}

void sthread::awake()
{
	// awaking the thread from its idle-loop to let it start searching

	search = true;
	cv.notify_one();
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
	// summing up the tablebase-hit count of all threads

	int64 hits{};
	for (auto& t : *pool) hits += t->cnt_tbhit;
	return hits;
}

void sthread::rearrange_pv(int mv_cnt)
{
	// sorting multiple principal variations

	verify(pv.size() == uci::multipv);
	auto pv_end{ std::min(pv.begin() + mv_cnt, pv.end()) };
	std::stable_sort(pv.begin(), pv_end, [&](move_var a, move_var b) { return a.sc > b.sc; });
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

void thread_pool::stop_search()
{
	// reacting to the UCI 'stop' command

	uci::stop = true;
	if (thread[0]->std_thread.joinable())
		thread[0]->std_thread.join();
}

void thread_pool::start_clock(const timemanage::move_time &movetime)
{
	// starting the clock in each thread

	for (auto t : thread) t->chrono.set(movetime);
}

void thread_pool::extend_time(score drop)
{
	// extending the search-time if the score is dropping

	for (auto t : thread) t->chrono.extend(drop);
}

void thread_pool::clear_history()
{
	// clearing the history tables only between new games but not between searches

	for (auto t : thread)
		t->hist.clear();
}

std::tuple<move, move> thread_pool::get_bestmove() const
{
	// looking for the best move at the end of the search
	// the best move of the thread that finished the highest iteration seems the most promising
	// the second move returned is the ponder move
	
	int index{};
	depth dt{};
	for (auto& t : thread)
	{
		if (t->pv[0].dt > dt && t->pv[0].mv[0])
		{
			dt = t->pv[0].dt;
			index = t->index;
		}
	}
	return std::make_tuple(thread[index]->pv[0].mv[0], thread[index]->pv[0].mv[1]);
}