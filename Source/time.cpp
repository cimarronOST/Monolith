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
#include "time.h"

int chronometer::hit_threshold{};

bool timemanage::move_time::infinite() const
{
	// returning true if the search is started without a time limit

	return target == lim::movetime;
}

bool timemanage::move_time::fixed() const
{
	// after the UCI command 'go movetime' the tolerable search-time is set to 0
	// the search should then use the target search-time as precisely as possible

	return tolerable == milliseconds(0) && !infinite();
}

timemanage::move_time timemanage::get_movetime(color cl)
{
	// calculating the optimal target search-time for the current move
	// additionally computing a tolerable bound as an upper limit for additional dynamic time allocation
	// when pondering is enabled it is assumed that more search-time is available through ponder-hits
	// with infinite analysis or fixed search-time, the calculation is skipped

	if (restricted)
		return { movetime.infinite() ? movetime.target : movetime.target - uci::overhead, milliseconds(0) };

	int  moves{ movestogo ? movestogo / 2 + 1 : 25 };
	auto max_time{ time[cl] - uci::overhead };
	auto target_time{ time[cl] / moves + incr[cl] - uci::overhead };
	if (uci::ponder)
		target_time = target_time * 4 / 3;
	auto tolerable_time{ target_time + (max_time - target_time) / 5 };
	tolerable_time = std::min(tolerable_time, max_time);

	// applying a safety margin and finalizing the search-time calculations

	movetime.tolerable = tolerable_time - std::max(std::min(target_time / 20, milliseconds(200)),  milliseconds(2));
	movetime.target    = std::max(std::min(target_time, movetime.tolerable), milliseconds(1));
	return movetime;
}

void chronometer::reset_hit_threshold()
{
	// resetting the threshold for how often the elapsed search-time gets checked
	// default is every 256 nodes

	hit_threshold = 256LL;
}

void chronometer::start()
{
	// starting the internal clock

	start_time = std::chrono::system_clock::now();
}

void chronometer::set(const timemanage::move_time& time)
{
	// resetting the counter & starting the clock

	hits = 0;
	movetime = time;
	start();
}

void chronometer::extend(score drop)
{
	// extending the search-time when the score is dropping

	if (movetime.infinite() || movetime.fixed())
		return;

	verify(drop <= -25);
	milliseconds target{ movetime.target * 6 / 5 };
	if (drop <= -50)
		target = target * 6 / 5;

	movetime.target = std::min(target, movetime.tolerable);
}

milliseconds chronometer::elapsed() const
{
	// computing the elapsed time since the start of the search 

	return std::chrono::duration_cast<milliseconds>(std::chrono::system_clock::now() - start_time);
}