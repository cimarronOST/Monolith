/*
  Monolith 2
  Copyright (C) 2017-2020 Jonas Mayr
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

#include "movepick.h"
#include "board.h"
#include "types.h"
#include "main.h"

/*
  probing Syzygy endgame tablebases
  all credits go to Ronald de Man for creating the tablebases and providing the probing code:
  https://github.com/syzygy1/tb
  https://github.com/syzygy1/Cfish
  the probing code has been modified to conform with the engine
  DTM tablebases have not been officially released yet, their probing code is therefore not complete
  32-bit is only supported for up to 5-piece tables
*/

namespace syzygy
{
	extern int pc_max;
	extern int tb_cnt;

    void init_tb(const std::string& path);

	// probing the tablebases

    int probe_wdl(board& pos, int& success);
    int probe_dtz(board& pos, int& success);

    bool probe_dtz_root(board& pos, rootpick& pick, const std::array<key64, 256>& rep_hash);
    bool probe_wdl_root(board& pos, rootpick& pick);
}