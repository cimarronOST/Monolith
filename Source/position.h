/*
  Monolith 0.3  Copyright (C) 2017 Jonas Mayr

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

// representing the board's position and all game states

class pos
{
public:

	// representing pieces

	uint64 pieces[6];
	uint64 side[3];
	uint8 piece_sq[64];
	int king_sq[2];

	// representing all other positional essentials

	int move_cnt;
	int half_move_cnt;
	int turn;
	int not_turn;
	uint64 ep_sq;
	bool castl_rights[4];

	// helping parameters for search & evaluation

	uint64 key;
	uint16 capture;
	uint8 phase;

	// manipulating the position

	void parse_fen(std::string fen);
	void new_move(uint32 move);
	void null_move(uint64 &ep_copy, uint16 &capt_copy);
	void undo_null_move(uint64 &ep_copy, uint16 &capt_copy);

	void rook_moved(uint64 &sq64, uint16 sq);
	void clear();

	bool lone_king() const;
	bool recapture(uint32 move) const;
};
