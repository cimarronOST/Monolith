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

#include "time.h"
#include "thread.h"
#include "board.h"
#include "types.h"
#include "main.h"

// searching for the best move
// all Elo estimates in the comments were done by removing the feature and doing 750 games with very fast time control
// therefore they are not very accurate

namespace search
{
	struct p_variation { std::array<move, lim::dt> mv; int cnt; };
	extern int64 bench;

    void init_tables();

    score qsearch(sthread& thread, board& pos, p_variation& pv, bool in_check, depth curr_dt, depth dt, score alpha, score beta);
    void start(thread_pool& threads, timemanage::move_time movetime);
}