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


#pragma once

#include <chrono>

#include "main.h"

// calculating the time per move

class chronos
{
public:

	chronos() : time{ 0 }, incr{ 0 }, moves_to_go{ 50 } { }

	int time[2];
	int incr[2];
	int moves_to_go;

	uint64 movetime;

	uint64 get_movetime(int turn);
	void set_movetime(uint64 new_time);
};

// internal clock

class chronometer
{
private:

	std::chrono::time_point<std::chrono::system_clock> start_time;

public:

	chronometer() { start(); }

	void start();
	uint64 elapsed() const;
};
