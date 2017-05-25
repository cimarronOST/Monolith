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

#include <vector>

#include "position.h"
#include "main.h"

using std::vector;

class movegen
{
public:

	movegen() { }
	movegen(pos &board, gen_e type)
	{
		gen_moves(board, type);
	}

	class legal
	{
	public:

		legal() { }
		legal(pos &board)
		{
			init_legal(board);
			init_pin(board);
		}
		~legal()
		{
			unpin();
		}

		static void pin_down(pos &board);
	};

	static void init();
	int gen_moves(pos &board, gen_e type);

	// movelist

	uint16 list[lim::movegen];
	int move_cnt;
	int promo_cnt;
	int capt_cnt;

	// movelist functions

	uint16 *find(uint16 move);
	bool in_list(uint16 move);

	// bitboards needed for generation

	static uint64 knight_table[64];
	static uint64 king_table[64];
	static uint64 pinned[64];
	static uint64 legal_sq;

	static uint64 slide_att(const int sl, const int sq, uint64 occ);
	static uint64 check(pos &board, int turn, uint64 squares);

private:

	// move generation

	void pawn_promo(pos &board);
	void pawn_capt(pos &board);
	void pawn_quiet(pos &board);
	void piece_moves(pos &board, gen_e type);
	void king_moves(pos &board, gen_e type);

	static void init_legal(pos &board);
	static void init_pin(pos &board);
	static void unpin();
};

namespace magic
{
	// initialise all arrays for magic move generation

	struct entry
	{
		size_t offset;
		uint64 mask;
		uint64 magic;
		int shift;
	};

	struct pattern
	{
		int shift;
		uint64 boarder;
	};

	void init();

	void init_mask(int sl);
	void init_blocker(int sl, vector<uint64> &blocker);
	void init_move(int sl, vector<uint64> &blocker, vector<uint64> &attack_temp);
	void init_magic(int sl, vector<uint64> &blocker, vector<uint64> &attack_temp);
	void init_connect(int sl, vector<uint64> &blocker, vector<uint64> &attack_temp);

	void init_ray(int sl);
	void init_king();
	void init_knight();
}

namespace attack
{
	uint64 by_pawns(pos &board, int turn);
}
