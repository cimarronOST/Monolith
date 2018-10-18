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

// arranging the internal move encoding

namespace move
{
	// decoding a move into 24 bytes

	struct elements
	{
		int sq1;
		int sq2;
		int flag;
		int piece;
		int victim;
		int turn;
	};

	static_assert(sizeof(elements) == 24, "decoded move != 24 bytes");

	int sq1(uint32 move);
	int sq2(uint32 move);
	int flag(uint32 move);
	int piece(uint32 move);
	int victim(uint32 move);
	int turn(uint32 move);

	elements decode(uint32 move);

	// encoding a move into 3 bytes (but storing it into 4 bytes)

	uint32 encode(int sq1, int sq2, int flag, int piece, int victim, int turn);

	// bundling the principal variation

	struct variation
	{
		uint32 move[lim::depth];
		int depth;
		int seldepth;
		int score;
		bool wrong;
	};

	// determining important move properties

	bool castling(int flag);
	bool castling(uint32 move);
	bool capture(uint32 move);
	bool promo(uint32 move);
	bool quiet(uint32 move);

	bool push_to_7th(uint32 move);
	bool pawn_advance(uint32 move);

	int promo_piece(uint32 move);
	int promo_piece(int flag);
	int castle_side(int flag);

	// converting the move into algebraic notation as specified by the UCI-protocol

	std::string algebraic(uint32 move);
}