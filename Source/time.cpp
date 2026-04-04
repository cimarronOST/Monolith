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


#include <chrono>
#include <algorithm>

#include "main.h"
#include "types.h"
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

	int moves{ movestogo ? movestogo / 2 + 1 : target_moves };
	moves += moves / (uci::mv_cnt / 2 + 1);
	auto target{ time[cl] / moves + incr[cl] - uci::overhead };
	if (uci::ponder)
		target = target * 4 / 3;

	auto max{ time[cl] - uci::overhead };
	auto tolerable{ target + (max - target) / tolerable_div };
	tolerable = std::min(tolerable, max);

	// applying a safety margin and finalizing the search-time calculations

	movetime.tolerable = tolerable - std::clamp(target / 20, milliseconds(2), milliseconds(200));
	movetime.target    = std::clamp(target, milliseconds(1), movetime.tolerable);
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

	milliseconds target{ movetime.target * extend_time / 8 };
	if (drop <= -50)
		target = target * extend_time / 8;

	movetime.target = std::min(target, movetime.tolerable);
}

milliseconds chronometer::elapsed() const
{
	// computing the elapsed time since the start of the search 

	return std::chrono::duration_cast<milliseconds>(std::chrono::system_clock::now() - start_time);
}