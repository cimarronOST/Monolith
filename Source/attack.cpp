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


#include "movegen.h"
#include "bitboard.h"
#include "magic.h"
#include "attack.h"

namespace
{
	const uint64 boarder[]{ file[A], file[H] };

	const int cap_left[]{ 9, 55 };
	const int cap_right[]{ 7, 57 };
}

uint64 attack::by_slider(const int sl, const int sq, uint64 occ)
{
	// magic index hashing function to generate sliding moves

	assert(sq >= 0 && sq < 64);
	assert(sl == ROOK || sl == BISHOP);

	occ  &= magic::slider[sl][sq].mask;
	occ  *= magic::slider[sl][sq].magic;
	occ >>= magic::slider[sl][sq].shift;
	return magic::attack_table[magic::slider[sl][sq].offset + static_cast<uint32>(occ)];
}

uint64 attack::check(const pos &board, int turn, uint64 all_sq)
{
	// returning the (king's) squares that are not under enemy attack

	assert(turn == WHITE || turn == BLACK);

	const uint64 king{ board.side[turn] & board.pieces[KINGS] };
	uint64 inquire{ all_sq };

	while (inquire)
	{
		auto sq{ bb::bitscan(inquire) };
		const uint64 sq64{ 1ULL << sq };
		const uint64 in_front[]{ ~(sq64 - 1), sq64 - 1 };

		uint64 att{ by_slider(ROOK, sq, board.side[BOTH] & ~king) & (board.pieces[ROOKS] | board.pieces[QUEENS]) };
		att |= by_slider(BISHOP, sq, board.side[BOTH] & ~king) & (board.pieces[BISHOPS] | board.pieces[QUEENS]);
		att |= movegen::knight_table[sq] & board.pieces[KNIGHTS];
		att |= movegen::king_table[sq] & board.pieces[KINGS];
		att |= movegen::king_table[sq] & board.pieces[PAWNS] & movegen::slide_ray[BISHOP][sq] & in_front[turn];
		att &= board.side[turn ^ 1];

		if (att)
			all_sq ^= sq64;

		inquire &= inquire - 1;
	}
	return all_sq;
}

uint64 attack::by_pawns(const pos &board, int col)
{
	// returning all squares attacked by pawns of int color

	assert(col == WHITE || col == BLACK);

	return shift(board.pieces[PAWNS] & board.side[col] & ~boarder[col], cap_left[col])
		 | shift(board.pieces[PAWNS] & board.side[col] & ~boarder[col ^ 1], cap_right[col]);
}

// SEE functions

uint64 attack::to_square(const pos &board, uint16 sq)
{
	// merging all attacking pieces of a specific square into one bitboard

	const uint64 in_front[]{ (1ULL << sq) - 1, ~((1ULL << sq) - 1) };

	uint64 att{ by_slider(ROOK, sq, board.side[BOTH]) & (board.pieces[ROOKS] | board.pieces[QUEENS]) };
	att |= by_slider(BISHOP, sq, board.side[BOTH]) & (board.pieces[BISHOPS] | board.pieces[QUEENS]);
	att |= movegen::knight_table[sq] & board.pieces[KNIGHTS];
	att |= movegen::king_table[sq] & board.pieces[KINGS];
	att |= movegen::king_table[sq] & board.pieces[PAWNS] & movegen::slide_ray[BISHOP][sq] & board.side[BLACK] & in_front[BLACK];
	att |= movegen::king_table[sq] & board.pieces[PAWNS] & movegen::slide_ray[BISHOP][sq] & board.side[WHITE] & in_front[WHITE];

	return att;
}

uint64 attack::add_xray(const pos &board, uint64 &occ, uint64 &set, uint64 &gone, uint16 sq)
{
	// adding undiscovered x-ray attackers to the attacker-set

	uint64 att{ by_slider(ROOK, sq, occ) & (board.pieces[ROOKS] | board.pieces[QUEENS]) };
	att |= by_slider(BISHOP, sq, occ) & (board.pieces[BISHOPS] | board.pieces[QUEENS]);
	att &= ~gone;

	assert(!((1ULL << sq) & att));
	assert(bb::popcnt((att & set) ^ att) <= 1);

	return (att & set) ^ att;
}
