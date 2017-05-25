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

#include "main.h"

class pos
{
public:
	uint64 pieces[6];
	uint64 side[3];
	uint8 piece_sq[64];
	int king_sq[2];

	int moves;
	int half_moves;
	int turn;
	uint64 ep_square;
	uint8 castl_rights;

	uint64 key;
	uint8 phase;
	static uint64 nodes;

	void parse_fen(string fen);
	void new_move(uint16 move);
	void null_move(uint64 &ep_copy);
	void undo_null_move(uint64 &ep_copy);

	void rook_moved(uint64 &sq64, uint16 sq);
	void clear();

	bool lone_king() const;
};
