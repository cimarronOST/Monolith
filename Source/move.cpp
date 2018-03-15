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

int move::sq1(uint32 move)
{
	return move & 0x3f;
}

int move::sq2(uint32 move)
{
	return (move >> 6) & 0x3f;
}

int move::flag(uint32 move)
{
	return (move >> 12) & 0xf;
}

int move::piece(uint32 move)
{
	return (move >> 16) & 0x7;
}

int move::victim(uint32 move)
{
	return (move >> 19) & 0x7;
}

int move::turn(uint32 move)
{
	assert(move >> 23 == 0);
	return move >> 22;
}

move::elements move::decode(uint32 move)
{
	return{ sq1(move), sq2(move), flag(move), piece(move), victim(move), turn(move) };
}

uint32 move::encode(int sq1, int sq2, int flag, int piece, int victim, int turn)
{
	assert(turn == (turn & 1));
	assert(piece <= 5);
	assert(victim <= 4 || victim == 7);

	return sq1 | (sq2 << 6) | (flag << 12) | (piece << 16) | (victim << 19) | (turn << 22);
}

bool move::is_castling(int flag)
{
	return flag == castling::SHORT || flag == castling::LONG;
}

bool move::is_castling(uint32 move)
{
	return is_castling(flag(move));
}

bool move::is_promo(uint32 move)
{
	return flag(move) >= PROMO_ALL;
}

bool move::is_quiet(uint32 move)
{
	return victim(move) == NONE && !is_promo(move);
}

bool move::is_push_to_7th(uint32 move)
{
	return piece(move) == PAWNS && relative::rank(square::rank(sq2(move)), turn(move)) == R7;
}

bool move::is_pawn_advance(uint32 move)
{
	return piece(move) == PAWNS && relative::rank(square::rank(sq2(move)), turn(move)) > R4;
}