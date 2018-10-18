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


#pragma once

#include "thread.h"
#include "position.h"
#include "main.h"

// analyzing the search & integrating a testing interface for debugging

namespace analysis
{
	void reset();
	void summary();

	// doing a performance- and correctness-test of the move-generator 

	void perft(board &pos, int depth, const gen_mode mode);
}

// searching for the best move, this is the heart of the engine

namespace search
{
	void reset();
	void start(thread_pool &threads, int64 movetime);
}