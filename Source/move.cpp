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


#include "utilities.h"
#include "uci.h"
#include "move.h"

namespace
{
	// simplifying algebraic notation of moves

	const std::string promo[]{ "n", "b", "r", "q" };
}

namespace
{
	std::string add_promo(int flag)
	{
		// adding the corresponding character for pawn promotion to the coordinate-string

		assert(flag >= NONE && flag <= PROMO_QUEEN);
		return flag >= PROMO_ALL ? promo[flag - PROMO_ALL] : "";
	}

	std::string coordinate(int sq)
	{
		// converting a square index of a bitboard into a coordinate-string

		assert(sq >= H1 && sq <= A8);
		std::string str{};
		str += 'h' - static_cast<char>(index::file(sq));
		str += '1' + static_cast<char>(index::rank(sq));
		return str;
	}
}

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
	auto flag{ (move >> 12) & 0xf };
	assert(flag >= NONE);
	return flag;
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
	auto turn{ move >> 22 };
	assert(turn == WHITE || turn == BLACK);
	return turn;
}

move::elements move::decode(uint32 move)
{
	return{ sq1(move), sq2(move), flag(move), piece(move), victim(move), turn(move) };
}

uint32 move::encode(int sq1, int sq2, int flag, int piece, int victim, int turn)
{
	assert(sq1    >= H1    && sq1    <= A8);
	assert(sq2    >= H1    && sq2    <= A8);
	assert(flag   >= NONE  && flag   <= PROMO_QUEEN);
	assert(piece  >= PAWNS && piece  <= KINGS);
	assert(victim >= PAWNS && victim <= NONE && victim != KINGS);
	assert(turn   == WHITE || turn   == BLACK);

	return sq1 | (sq2 << 6) | (flag << 12) | (piece << 16) | (victim << 19) | (turn << 22);
}

bool move::castling(int flag)
{
	assert(flag >= NONE && flag <= PROMO_QUEEN);
	return flag == CASTLE_SHORT || flag == CASTLE_LONG;
}

bool move::castling(uint32 move)
{
	return castling(flag(move));
}

bool move::capture(uint32 move)
{
	return victim(move) != NONE;
}

bool move::promo(uint32 move)
{
	return flag(move) >= PROMO_ALL;
}

bool move::quiet(uint32 move)
{
	return victim(move) == NONE && !promo(move);
}

bool move::push_to_7th(uint32 move)
{
	return piece(move) == PAWNS && relative::rank(index::rank(sq2(move)), turn(move)) == R7;
}

bool move::pawn_advance(uint32 move)
{
	return piece(move) == PAWNS && relative::rank(index::rank(sq2(move)), turn(move)) >= R5;
}

int move::promo_piece(uint32 move)
{
	return promo_piece(flag(move));
}

int move::promo_piece(int flag)
{
	assert(flag >= PROMO_ALL && flag <= PROMO_QUEEN);
	return flag - 11;
}

int move::castle_side(int flag)
{
	assert(castling(flag));
	static_assert(CASTLE_SHORT == 10, "castle flag");
	return flag - 10;
}

std::string move::algebraic(uint32 move)
{
	// converting <move> to algebraic notation, e.g. 'e2e4'

	if (move == MOVE_NONE)
		return "0000";
	elements el{ decode(move) };

	// adjusting castling notation

	if (!uci::chess960 && castling(el.flag))
		el.sq2 = square::king_target[el.turn][castle_side(el.flag)];

	return coordinate(el.sq1) + coordinate(el.sq2) + add_promo(el.flag);
}