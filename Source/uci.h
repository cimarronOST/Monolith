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

#include <sstream>
#include <thread>

#include "position.h"
#include "main.h"

// universal chess interface communication protocol

namespace uci
{
	const std::string version_number{ "0.4" };

	void loop();

	void search(board *pos, uint64 time);
	void searchmoves(std::istringstream &stream, board &pos);
	void go(std::istringstream &stream, std::thread &searching, board &pos);

	void show_options(board &pos);
	void setoption(std::istringstream &stream, board &pos);
	void position(std::istringstream &stream, board &pos);
}