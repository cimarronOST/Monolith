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


#include <numeric>

#include "uci.h"
#include "search.h"
#include "thread.h"

std::atomic<int> thread_pool::searching{};
std::mutex thread_pool::mutex;
std::condition_variable thread_pool::cv;

void sthread::idle_loop()
{
	// arranging the thread's cycle of searching and waiting

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

	assert(!main && !search && !exit);
	std_thread = std::thread{ &sthread::idle_loop, this };
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

	return std::accumulate(pool->begin(), pool->end(), 0LL,
		[](int64 &nodes, const sthread *t) { return nodes + t->nodes; });
}

int64 sthread::get_tbhits() const
{
	// summing up the tablebase-hit count of all threads

	return std::accumulate(pool->begin(), pool->end(), 0LL,
		[](int64 &hits, const sthread *t) { return hits + t->count.tbhit; });
}

void thread_pool::resize(uint32 size)
{
	// adjusting the number of search-threads

	assert(size <= lim::threads);
	while (thread.size() > 0)
	{
		delete thread.back();
		thread.pop_back();
	}

	while (thread.size() < size)
		thread.push_back(new sthread(pos, &this->thread));

	if (thread.size() >= 1)
		thread[MAIN]->main = true;
}

void thread_pool::start_all() const
{
	// starting all search-threads except the main search-thread

	for (uint32 i{ 1 }; i < thread.size(); ++i)
		thread[i]->start();
}

void thread_pool::stop_search()
{
	// reacting to the UCI 'stop' command

	uci::stop = true;
	if (thread[MAIN]->std_thread.joinable())
		thread[MAIN]->std_thread.join();
}

void thread_pool::start_clock(int64 &movetime)
{
	// starting the clock in each thread

	for (auto t : thread) t->chrono.set(movetime);
}
