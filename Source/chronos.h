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


#pragma once

#include <chrono>

#include "main.h"

// allocating the searchtime

class chronomanager
{
public:

	chronomanager() : time{ }, incr{ }, moves_to_go{ 50 }, movetime{ lim::movetime }, infinite{ true } { }

	int time[2];
	int incr[2];
	int moves_to_go;
	uint64 movetime;
	bool infinite;

	uint64 get_movetime(int turn);
};

// internal clock

class chronometer
{
private:

	std::chrono::time_point<std::chrono::system_clock> start_time;

public:

	chronometer() { start(); }

	int hits{ };
	uint64 max{ };
	uint64 elapsed{ };

	void start();
	uint64 split();
};