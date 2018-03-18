/*
  Monolith 0.4  Copyright (C) 2017 Jonas Mayr

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


#include "engine.h"
#include "chronos.h"

uint64 chronomanager::get_movetime(int turn)
{
	// calculating the ideal search time for the current move

	if (infinite)
		return movetime - engine::overhead;

	auto max_time{ time[turn] + incr[turn] * (moves_to_go - 1) - engine::overhead };
	auto target_time{ time[turn] / moves_to_go + incr[turn] - engine::overhead };

	// assuming that more time is available through ponderhits

	if (engine::ponder)
		target_time = target_time * 4 / 3;

	// applying a safety margin
	// considering also bullet time controls

	target_time = std::min(target_time, max_time);
	movetime = target_time - std::min(target_time / 20, 50);

	return std::max(movetime, 1ULL);
}

void chronometer::start()
{
	// starting the internal clock

	start_time = std::chrono::system_clock::now();
}

uint64 chronometer::split()
{
	// returning the elapsed time since start() in milliseconds

	elapsed = std::chrono::duration_cast<std::chrono::duration<uint64, std::ratio<1, 1000>>>
		(std::chrono::system_clock::now() - start_time).count();
	return elapsed;
}
