/*
  Monolith 0.1  Copyright (C) 2017 Jonas Mayr

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

#include <vector>

#include "position.h"
#include "main.h"

using std::vector;

class movegen
{
public:
	movegen() {}
	movegen(pos &board, gen_e type)
	{
		gen_moves(board, type);
	}

	static void init();
	int gen_moves(pos &board, gen_e type);

	uint16 movelist[lim::movegen];
	int move_cnt;

private:
	void pawn_promo(pos &board);
	void pawn_capt(pos &board);
	void pawn_quiet(pos &board);
	void piece_moves(pos &board, gen_e type);
	void king_moves(pos &board, gen_e type);

	void legal(pos &board);
	void pin(pos &board);
	void unpin();

	static uint64 slide_att(const uint8 sl, const int sq, uint64 occ);

public:
	static uint64 check(pos &board, int turn, uint64 squares);
	uint16 *find(uint16 move);
	bool in_list(uint16 move);
};


namespace magic
{
	struct magic_s
	{
		size_t offset;
		uint64 mask;
		uint64 magic;
		int shift;
	};

	void init();
	void init_mask(uint8 sl);
	void init_blocker(uint8 sl, vector<uint64> &blocker);
	void init_move(uint8 sl, vector<uint64> &blocker, vector<uint64> &attack_temp);
	void init_magic(uint8 sl, vector<uint64> &blocker, vector<uint64> &attack_temp);
	void init_connect(uint8 sl, vector<uint64> &blocker, vector<uint64> &attack_temp);

	void init_ray(uint8 sl);
	void init_king();
	void init_knight();
}