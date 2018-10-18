/*
  Monolith 1.0
  Copyright (C) 2011-2015 Ronald de Man
  Copyright (C) 2017-2018 Jonas Mayr

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
#include "position.h"
#include "main.h"

// probing syzygy endgame tablebases
// all credits go to Ronald de Man for creating the awesome tablebases and providing the probing code:
// https://github.com/syzygy1/tb
// the probing code has been modified to conform with the engine

namespace syzygy
{
	extern int max_pieces;
	extern int tablebases;

	void init_tablebases(std::string &path);

	// probing the tablebases

	int probe_wdl(board &pos, int &success);
	int probe_dtz(board &pos, int &success);

	bool probe_dtz_root(board &pos, rootpick &pick, uint64 repetition_hash[], int &tb_score);
	bool probe_wdl_root(board &pos, rootpick &pick, int &tb_score);
}