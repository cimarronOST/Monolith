/*
  Monolith 0.4  Copyright (C) 2017 Jonas Mayr

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


#include "move.h"
#include "engine.h"
#include "notation.h"

namespace
{
	const std::string postfix[]
	{ "n", "b", "r", "q" };

	const uint8 castling_sq2[][2]
	{ { G1, C1 }, { G8, C8 } };
}

namespace
{
	std::string to_promo(int flag)
	{
		assert(flag > 0 && flag <= 15);
		return flag >= 12 ? postfix[flag - 12] : "";
	}
}

std::string notation::to_str(int sq)
{
	std::string str;
	str += 'h' - static_cast<char>(square::file(sq));
	str += '1' + static_cast<char>(square::rank(sq));
	return str;
}

std::string notation::algebraic(uint32 move)
{
	// converting the internally encoded <move> to algebraic notation

	if (move == NO_MOVE)
		return "0000";

	move::elements el{ move::decode(move) };

	// adjusting castling notation

	if (!engine::chess960 && move::is_castling(el.flag))
		el.sq2 = castling_sq2[el.turn][el.flag - 10];

	return to_str(el.sq1) + to_str(el.sq2) + to_promo(el.flag);
}