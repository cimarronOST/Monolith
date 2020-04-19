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


#pragma once

#include "types.h"
#include "main.h"

// allocating the optimal searchtime for the current move

class timemanage
{
public:
	std::array<milliseconds, 2> time{};
	std::array<milliseconds, 2> incr{};
	int movestogo{};
	bool restricted{ true };

	struct move_time
	{
		milliseconds target;
		milliseconds tolerable;
		bool infinite() const;
		bool fixed() const;
	} movetime{ lim::movetime, milliseconds(0) };

	move_time get_movetime(color cl);
};

// accurate internal clock

class chronometer
{
private:
	std::chrono::time_point<std::chrono::system_clock> start_time{};

public:
	chronometer() { start(); }

	int hits{};
	timemanage::move_time movetime{};

	static int hit_threshold;
	static void reset_hit_threshold();

	void start();
	void set(const timemanage::move_time& movetime);
	void extend(score drop);
	milliseconds elapsed() const;
};