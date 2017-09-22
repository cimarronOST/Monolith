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

#include "position.h"
#include "main.h"

// move generation, can be staged and legal or pseudolegal

class movegen
{
public:

	pos &board;
	GEN_MODE mode;

	uint32 hash_move;

	movegen(pos &basic_board) : board(basic_board) { }

	movegen(pos &basic_board, GEN_MODE genmode) : board(basic_board), mode(genmode)
	{
		init_list();
	}

	movegen(pos &basic_board, GEN_MODE genmode, uint32 tt_move) : board(basic_board), mode(genmode), hash_move(tt_move)
	{
		init_list();
	}

private:

	// preparing legal moves

	uint64 pin[64]{ };
	uint64 evasions;

	void find_pins();
	void find_evasions();

public:

	// checking pseudo-legal moves

	bool is_pseudolegal(uint32 move);

	// concerning movelist

	uint32 movelist[lim::movegen];

	struct count
	{
		int moves{ 0 };
		int hash{ 0 };
		int capture{ 0 };
		int promo{ 0 };
		int quiet{ 0 };
		int loosing{ 0 };
	} cnt;

	bool in_list(uint32 move);
	uint32 *find(uint32 move);

	// bitboard filling

	static void init_ray(int sl);
	static void init_king();
	static void init_knight();

	static uint64 slide_ray[2][64];
	static uint64 knight_table[64];
	static uint64 king_table[64];

	// generating moves

	void init_list();

	int gen_hash();
	int gen_tactical();
	int gen_quiet();
	int gen_loosing();

private:

	void pawn_promo();
	void pawn_capt();
	void pawn_quiet();

	void knight(GEN_STAGE stage);
	void bishop(GEN_STAGE stage);
	void rook(GEN_STAGE stage);
	void queen(GEN_STAGE stage);
	void king(GEN_STAGE stage);

	// often used variables

	uint64 enemies, friends, fr_king, pawn_rank;
	uint64 not_right, not_left;
	uint64 gentype[2];

	uint8 AHEAD, BACK;
};
