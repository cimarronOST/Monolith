/*
  Monolith 2 Copyright (C) 2017-2020 Jonas Mayr
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

#include "move.h"
#include "board.h"
#include "types.h"
#include "main.h"

// Zobrist hash keys
// used for repetition detection, transposition table, pawn hash table and endgame tablebases

namespace zobrist
{
	// to make a position unique: piece placement, castling rights, en-passant and side to move

	extern std::array<std::array<std::array<key64, 64>, 6>, 2>
		key_pc;
	extern std::array<std::array<key64, 2>, 2>
		key_castle;
	extern std::array<key64, 8>
		key_ep;
	extern key64
		key_cl;

	// creating hash keys

	void init_keys();
	
	key64 pos_key(const board& pos);
	key64 pos_key(const board& pos, move& mv);
	key64 adjust_key(const key64& key, move& mv);
	key64 kingpawn_key(const board& pos);

	template<typename pc_list>
	key64 mat_key(pc_list& p, bool mirror);
	key64 mat_key(uint8 piece[], int cnt);
	key32 mv_key(move mv, key64& pos_key);
}