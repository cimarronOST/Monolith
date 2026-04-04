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

#include "board.h"
#include "move.h"
#include "types.h"

// piece attacking functions using bitboards

namespace attack
{
	// pins are pieces that are pinned to the king by a sliding piece
	// they have to be computed only for legal move generation

	class pin_mv
	{
	private:
		int pin_cnt{};
		std::array<int8, 64> pin_lc{};
		std::array<bit64, 9> pin{};

	public:
		bit64 operator[](square sq) const;
		void find(const board &pos, color cl_king, color cl_pc);
		void add(square sq, bit64 bb);
	};

	// value of all piece types (plus NO_PIECE) in centipawn units

	inline std::array<score, 7> value;

	// detecting check & finding evasions
	// evasions have to be computed only for legal move generation

	bit64 safe(const board &pos, color cl, bit64 mask);
	bit64 evasions(const board &pos);

	// generating attacks

	bit64 by_piece(piece pc, square sq, color cl, const bit64 &occ);
	template<piece pc>
	bit64 by_slider(square sq, const bit64& occ);
	bit64 by_pawns(bit64 pawns, color cl);
	bit64 sq(const board &pos, square sq, const bit64 &occ);

	// static exchange evaluation

	bool see_above(const board& pos, move new_mv, score margin);
	bool escape(const board &pos, move new_mv);
}