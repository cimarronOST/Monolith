/*
  Monolith Copyright (C) 2017-2026 Jonas Mayr

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

#include <array>

#include "move.h"
#include "board.h"
#include "types.h"

// Zobrist hash keys
// used for repetition detection, transposition table, pawn-king evaluation table,
// evaluation history correction tables and Syzygy endgame table-bases

namespace zobrist
{
	// to make a position unique: piece placement, castling rights, en-passant and side to move

	inline std::array<std::array<std::array<key64, 64>, 6>, 2>
		key_pc;
	inline std::array<std::array<key64, 2>, 2>
		key_castle;
	inline std::array<key64, 8>
		key_ep;
	inline key64
		key_cl;

	// creating hash keys

	void init_keys();
	
	key64 pos_key(const board& pos);
	key64 pos_key(const board& pos, move& mv);
	key64 adjust_key(const key64& key, move& mv);
	key64 pawn_key(const board& pos);
	key64 nonpawn_key(const board& pos, color cl);
	key64 kingpawn_key(const board& pos);
	key64 minor_key(const board& pos);
	key64 major_key(const board& pos);

	// creating hash keys for Syzygy endgame table-bases

	template<typename pc_list>
	key64 mat_key(pc_list& p, bool mirror);
	key64 mat_key(uint8 piece[], int cnt);
}