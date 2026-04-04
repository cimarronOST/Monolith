/*
  Monolith Copyright (C) 2017-2026 Jonas Mayr

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

#include <string>

#include "movepick.h"
#include "board.h"

/*
  probing Syzygy endgame table-bases
  credits go to Ronald de Man for creating the table-bases and providing the probing code:
  https://github.com/syzygy1/tb
  https://github.com/syzygy1/Cfish
  the probing code has been modified to conform with the engine
  DTM table-bases have not been officially released yet, their probing code is therefore not complete
  32-bit is only supported for up to 5-piece tables
*/

namespace syzygy
{
	inline int pc_max{};
	inline int tb_cnt{};

    void init_tb(const std::string& path);

	// probing the table-bases

    int  probe_wdl(board& pos, int& success);
    bool probe_root(board& pos, rootpick& pick);
}