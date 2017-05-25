/*
  Monolith 0.2  Copyright (C) 2017 Jonas Mayr

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

#include "position.h"
#include "main.h"

namespace conv
{
	uint64 to_bb(const string sq);
	int to_int(const string sq);

	string to_str(const uint64 &sq);
	string to_str(const int sq);

	uint16 san_to_move(pos &board, string move);
	string bit_to_san(pos &board, const uint64 &sq1, const uint64 &sq2, uint8 flag);

	string to_promo(uint8 flag);
	uint8 to_flag(char promo, const pos &board, const uint64 &sq1, const uint64 &sq2);
}
