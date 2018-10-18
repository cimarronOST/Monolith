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

#include "main.h"

// representing the piece placement and all board states
// essentially this is the virtual chessboard

class board
{
public:

	// representing pieces

	uint64 pieces[6];
	uint64 side[3];
	uint8 piece[64];
	uint8 sq_king[2];

	// representing all other positional essentials

	int move_count;
	int half_count;
	int turn;
	int xturn;
	uint64 ep_rear;
	uint8 castling_right[4];

	// keeping search & evaluation parameters updated with every move

	uint64 key;
	uint64 pawn_key;
	uint16 capture;

	// setting up new position

	void parse_fen(std::string fen);
	uint8 sq_rook(int col, int dir) const;

	// moving

	void new_move(uint32 move);
	void revert(board &prev_pos);
	void null_move(uint64 &ep_copy, uint16 &capture_copy);
	void revert_null_move(uint64 &ep_copy, uint16 &capture_copy);

private:

	void rook_is_engaged(int sq, int piece);
	void rook_castling(int sq1, int sq2);
	void clear();

public:

	// giving additional information to the search

	bool check() const;
	bool gives_check(uint32 move);
	bool lone_king() const;
	bool recapture(uint32 move) const;

	bool repetition(uint64 hash[], int offset) const;
	bool draw(uint64 hash[], int offset) const;

	// checking for legality

	bool pseudolegal(uint32 move) const;
	bool legal() const;
};
