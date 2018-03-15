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


#include "bit.h"
#include "magic.h"
#include "attack.h"

uint64 attack::in_front[2][64]{ };
uint64 attack::slide_map[2][64]{ };
uint64 attack::knight_map[64]{ };
uint64 attack::king_map[64]{ };

namespace
{
	// directing pawn captures

	const uint64 boarder_left[] { bit::file[A], bit::file[H] };
	const uint64 boarder_right[]{ bit::file[H], bit::file[A] };

	const int cap_left[] { 9, 55 };
	const int cap_right[]{ 7, 57 };
}

namespace
{
	// initialising tables at startup

	void init_slide(int sq, uint64 &sq64, int sl)
	{
		assert(sl == ROOK || sl == BISHOP);

		for (int dir{ sl }; dir < 8; dir += 2)
		{
			uint64 flood{ sq64 };
			while (!(flood & magic::ray[dir].boarder))
			{
				bit::real_shift(flood, magic::ray[dir].shift);
				attack::slide_map[sl][sq] |= flood;
			}
		}
	}

	void init_king(int sq, uint64 &sq64)
	{
		for (auto dir{ 0 }; dir < 8; ++dir)
		{
			if (!(sq64 & magic::ray[dir].boarder))
				attack::king_map[sq] |= bit::shift(sq64, magic::ray[dir].shift);
		}
	}

	void init_knight(int sq, uint64 &sq64, magic::pattern jump[])
	{
		for (auto dir{ 0 }; dir < 8; ++dir)
		{
			if (!(sq64 & jump[dir].boarder))
				attack::knight_map[sq] |= bit::shift(sq64, jump[dir].shift);
		}
	}

	void init_in_front(int sq, uint64 &sq64)
	{
		attack::in_front[BLACK][sq] =  (sq64 - 1) & ~bit::rank[square::rank(sq)];
		attack::in_front[WHITE][sq] = ~(sq64 - 1) & ~bit::rank[square::rank(sq)];
	}
}

void attack::fill_tables()
{
	// filling various attack tables, called at startup
	// the jump pattern is used for the knight table generation

	magic::pattern jump[]
	{
		{ 15, 0xffff010101010101 },{  6, 0xff03030303030303 },
		{ 54, 0x03030303030303ff },{ 47, 0x010101010101ffff },
		{ 49, 0x808080808080ffff },{ 58, 0xc0c0c0c0c0c0c0ff },
		{ 10, 0xffc0c0c0c0c0c0c0 },{ 17, 0xffff808080808080 }
	};

	for (int sq{ H1 }; sq <= A8; ++sq)
	{
		auto sq64{ 1ULL << sq };

		init_slide(sq, sq64, ROOK);
		init_slide(sq, sq64, BISHOP);
		init_king(sq, sq64);
		init_knight(sq, sq64, jump);
		init_in_front(sq, sq64);
	}
}

uint64 attack::check(const board &pos, int turn, uint64 all_sq)
{
	// returning the squares <all_sq> that are not under enemy attack

	assert(turn == WHITE || turn == BLACK);

	auto occ{ pos.side[BOTH] & ~(pos.pieces[KINGS] & pos.side[turn]) };
	auto inquire{ all_sq };

	while (inquire)
	{
		auto sq{ bit::scan(inquire) };
		auto att
		{
			pos.side[turn ^ 1]
			& ((pos.pieces[PAWNS] & king_map[sq] & slide_map[BISHOP][sq] & in_front[turn][sq])
			|  (pos.pieces[KNIGHTS] & knight_map[sq])
			| ((pos.pieces[ROOKS] | pos.pieces[QUEENS]) & by_slider<ROOK>(sq, occ))
			| ((pos.pieces[BISHOPS] | pos.pieces[QUEENS]) & by_slider<BISHOP>(sq, occ))
			|  (pos.pieces[KINGS] & king_map[sq]))
		};
		if (att)
			all_sq ^= (1ULL << sq);
		inquire &= inquire - 1;
	}

	return all_sq;
}

template uint64 attack::by_slider<ROOK>  (int sq, uint64 occ);
template uint64 attack::by_slider<BISHOP>(int sq, uint64 occ);
template uint64 attack::by_slider<QUEEN> (int sq, uint64 occ);

template<sliding_type sl>
uint64 attack::by_slider(int sq, uint64 occ)
{
	// magic index hashing function to generate sliding moves

	if (sl == QUEEN)
		return by_slider<ROOK>(sq, occ) | by_slider<BISHOP>(sq, occ);

	assert(sq >= H1 && sq <= A8);
	assert(sl == ROOK || sl == BISHOP);

	occ  &= magic::slider[sl][sq].mask;
	occ  *= magic::slider[sl][sq].magic;
	occ >>= magic::slider[sl][sq].shift;
	occ  += magic::slider[sl][sq].offset;
	return  magic::attack_table[occ];
}

uint64 attack::by_pawns(const board &pos, int turn)
{
	// returning all squares attacked by pawns

	assert(turn == WHITE || turn == BLACK);

	return bit::shift(pos.pieces[PAWNS] & pos.side[turn] & ~boarder_left[turn],  cap_left[turn])
		 | bit::shift(pos.pieces[PAWNS] & pos.side[turn] & ~boarder_right[turn], cap_right[turn]);
}

// assisting the SEE-algorithm

uint64 attack::to_square(const board &pos, int sq)
{
	// merging all attacking pieces of a square <sq> into one bitboard

	return (pos.pieces[PAWNS] & king_map[sq] & slide_map[BISHOP][sq] & pos.side[BLACK] & in_front[WHITE][sq])
		 | (pos.pieces[PAWNS] & king_map[sq] & slide_map[BISHOP][sq] & pos.side[WHITE] & in_front[BLACK][sq])
		 | (pos.pieces[KNIGHTS] & knight_map[sq])
		 |((pos.pieces[BISHOPS] | pos.pieces[QUEENS]) & by_slider<BISHOP>(sq, pos.side[BOTH]))
		 |((pos.pieces[ROOKS] | pos.pieces[QUEENS]) & by_slider<ROOK>(sq, pos.side[BOTH]))
		 | (pos.pieces[KINGS] & king_map[sq]);
}

uint64 attack::add_xray(const board &pos, int sq, uint64 &occ)
{
	// adding newly uncovered x-ray attackers to the attacker-set

	return ((by_slider<ROOK>(sq, occ) & (pos.pieces[ROOKS] | pos.pieces[QUEENS]))
		| (by_slider<BISHOP>(sq, occ) & (pos.pieces[BISHOPS] | pos.pieces[QUEENS])))
		& occ;
}