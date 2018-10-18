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


#include "uci.h"
#include "chronos.h"

int64 chronomanager::get_movetime(int turn)
{
	// calculating the ideal search time for the current move

	if (infinite)
		return movetime - uci::overhead;

	auto max_time{ time[turn] + incr[turn] * (moves_to_go - 1) - uci::overhead * moves_to_go };
	auto target_time{ time[turn] / moves_to_go + incr[turn] - uci::overhead };

	// assuming that more time is available through ponder-hits with pondering enabled

	if (uci::ponder)
		target_time = target_time * 4 / 3;

	// applying a safety margin
	// considering also bullet time controls

	target_time = std::min(target_time, max_time);
	movetime = int64{ target_time - std::min(target_time / 20, 100) };

	return std::max(movetime, 1LL);
}

void chronometer::start()
{
	// starting the internal clock

	start_time = std::chrono::system_clock::now();
}

void chronometer::set(int64 &movetime)
{
	// resetting the counter & starting the clock

	hits = 0;
	max = movetime;
	start();
}

int64 chronometer::elapsed() const
{
	// returning the elapsed time since start() in milliseconds

	return std::chrono::duration_cast<std::chrono::duration<int64, std::ratio<1, 1000>>>
		(std::chrono::system_clock::now() - start_time).count();
}