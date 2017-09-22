/*
  Monolith 0.3  Copyright (C) 2017 Jonas Mayr

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


#include "chronos.h"

uint64 chronos::get_movetime(int turn)
{
	if (movetime) return movetime;

	// applying a safety margin on the movetime

	movetime = time[turn] / moves_to_go + incr[turn];
	movetime -= movetime / 20 + incr[turn] / moves_to_go;

	return movetime;
}

void chronos::set_movetime(uint64 new_time)
{
	movetime = new_time - new_time / 30;
}

void chronometer::start()
{
	start_time = std::chrono::system_clock::now();
}

uint64 chronometer::elapsed() const
{
	return std::chrono::duration_cast<std::chrono::duration<uint64, std::ratio<1, 1000>>>
		(std::chrono::system_clock::now() - start_time).count();
}
