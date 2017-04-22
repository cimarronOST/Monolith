/*
  Monolith 0.1  Copyright(C) 2017 Jonas Mayr

  This file is part of Monolith.

  Monolith is free software : you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Monolith is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with Monolith.If not, see <http://www.gnu.org/licenses/>.
*/


#pragma once

#include "position.h"
#include "main.h"

class game
{
public:
	static int moves;
	static string game_str;
	static uint16 movelist[];
	static uint64 hashlist[];

	static void reset();
	static void save_move(pos &board, uint16 move);
};

namespace draw
{
	bool verify(pos &board, uint64 list[]);
	bool by_material(pos &board);
	bool by_rep(pos &board, uint64 list[], int size);
}