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


#if defined(PEXT)
  #include <immintrin.h>
#endif

#include "move.h"
#include "utilities.h"
#include "bit.h"
#include "magic.h"
#include "attack.h"

uint64 attack::in_front[2][64]{};
uint64 attack::slide_map[2][64]{};
uint64 attack::knight_map[64]{};
uint64 attack::king_map[64]{};

namespace attack
{
	// initializing tables at startup

	void init_slide(int sq, uint64 &sq_bit, int sl)
	{
		assert(sl == ROOK || sl == BISHOP);

		for (int dir{ sl }; dir < 8; dir += 2)
		{
			uint64 flood{ sq_bit };
			while (!(flood & magic::ray[dir].boarder))
			{
				bit::real_shift(flood, magic::ray[dir].shift);
				slide_map[sl][sq] |= flood;
			}
		}
	}

	void init_king(int sq, uint64 &sq_bit)
	{
		for (auto &ray : magic::ray)
			king_map[sq] |= sq_bit & ray.boarder ? 0ULL : bit::shift(sq_bit, ray.shift);
	}

	void init_knight(int sq, uint64 &sq_bit)
	{
		static magic::pattern jump[]
		{
			{ 15, 0xffff010101010101 },{  6, 0xff03030303030303 },
			{ 54, 0x03030303030303ff },{ 47, 0x010101010101ffff },
			{ 49, 0x808080808080ffff },{ 58, 0xc0c0c0c0c0c0c0ff },
			{ 10, 0xffc0c0c0c0c0c0c0 },{ 17, 0xffff808080808080 }
		};

		for (auto &j : jump)
			knight_map[sq] |= sq_bit & j.boarder ? 0ULL : bit::shift(sq_bit, j.shift);
	}

	void init_in_front(int sq, uint64 &sq_bit)
	{
		in_front[BLACK][sq] =  (sq_bit - 1) & ~bit::rank[index::rank(sq)];
		in_front[WHITE][sq] = ~(sq_bit - 1) & ~bit::rank[index::rank(sq)];
	}

	// assisting SEE

	int sum_gain(int gain[], int depth)
	{
		// summing up the final exchange balance score

		while (--depth)
			gain[depth - 1] = -std::max(-gain[depth - 1], gain[depth]);
		return gain[0];
	}

	uint64 least_valuable(const board &pos, uint64 set, int &piece)
	{
		// selecting the least valuable attacker of the set

		for (auto &p : pos.pieces)
		{
			if (p & set)
			{
				auto sq1{ bit::scan(p & set) };
				piece = pos.piece[sq1];
				return 1ULL << sq1;
			}
		}
		return 0ULL;
	}

	uint64 to_square(const board &pos, int sq, const uint64 &occ)
	{
		// finding all attacking pieces of a square

		return (pos.pieces[PAWNS] & king_map[sq] & slide_map[BISHOP][sq] & pos.side[BLACK] & in_front[WHITE][sq])
			|  (pos.pieces[PAWNS] & king_map[sq] & slide_map[BISHOP][sq] & pos.side[WHITE] & in_front[BLACK][sq])
			|  (pos.pieces[KNIGHTS] & knight_map[sq])
			| ((pos.pieces[BISHOPS] | pos.pieces[QUEENS]) & by_slider<BISHOP>(sq, occ))
			| ((pos.pieces[ROOKS]   | pos.pieces[QUEENS]) & by_slider<ROOK>(sq, occ))
			|  (pos.pieces[KINGS] & king_map[sq]);
	}

	uint64 add_xray_attacker(const board &pos, int sq, const uint64 &occ)
	{
		// adding uncovered x-ray attackers to the attacker-set

		return ((by_slider<ROOK>(sq, occ) & (pos.pieces[ROOKS] | pos.pieces[QUEENS]))
			| (by_slider<BISHOP>(sq, occ) & (pos.pieces[BISHOPS] | pos.pieces[QUEENS]))) & occ;
	}
}

void attack::fill_tables()
{
	// filling various attack tables, called at startup

	for (int sq{ H1 }; sq <= A8; ++sq)
	{
		auto sq_bit{ 1ULL << sq };
		init_slide(sq, sq_bit, ROOK);
		init_slide(sq, sq_bit, BISHOP);
		init_king(sq, sq_bit);
		init_knight(sq, sq_bit);
		init_in_front(sq, sq_bit);
	}
}

uint64 attack::check(const board &pos, int turn, uint64 all_sq)
{
	// returning all squares that are not under enemy attack

	assert(turn == WHITE || turn == BLACK);

	auto occ{ pos.side[BOTH] & ~(pos.pieces[KINGS] & pos.side[turn]) };
	auto inquire{ all_sq };
	while (inquire)
	{
		auto sq{ bit::scan(inquire) };
		if (attack::to_square(pos, sq, occ) & pos.side[turn ^ 1])
			all_sq ^= (1ULL << sq);
		inquire &= inquire - 1;
	}
	return all_sq;
}

uint64 attack::evasions(const board &pos)
{
	// defining the evasion zone if the king is under attack

	uint64 evasions{};
	auto sq{ pos.sq_king[pos.turn] };
	auto attacker(attack::to_square(pos, sq, pos.side[BOTH]) & pos.side[pos.xturn]);

	switch (bit::popcnt(attacker))
	{
	case 0: // no evasion move necessary

		evasions = std::numeric_limits<uint64>::max();
		break;

	case 1: // only evasions that block the path or capture the attacker are legal

		if (attacker & pos.pieces[KNIGHTS] || attacker & pos.pieces[PAWNS])
			evasions = attacker;
		else
		{
			assert(attacker & (pos.pieces[ROOKS] | pos.pieces[BISHOPS] | pos.pieces[QUEENS]));
			auto rays_attacker{ by_slider<QUEEN>(sq, pos.side[BOTH]) };
			auto sq_bit{ 1ULL << sq };

			for (auto &ray : magic::ray)
			{
				auto flood{ sq_bit };
				for ( ; !(flood & ray.boarder); flood |= bit::shift(flood, ray.shift));
				if (flood & attacker)
				{
					evasions = flood & rays_attacker;
					break;
				}
			}
		}
		break;

	default: // 2 attackers, only king move evasions may be legal

		assert(bit::popcnt(attacker) == 2);
		evasions = 0ULL;
		break;
	}
	return evasions;
}

void attack::pins(const board &pos, int side_king, int side_pin, uint64 pin_moves[])
{
	// finding all pieces that are pinned to the king and defining their legal move zone
	// i.e. pins of pieces of side_pin to the king of side_king

	auto side_enemy{ side_king ^ 1 };
	auto king_sq_bit{ pos.pieces[KINGS] & pos.side[side_king] };
	auto all_attacker
	{
		pos.side[side_enemy] & (((pos.pieces[BISHOPS] | pos.pieces[QUEENS]) & slide_map[BISHOP][pos.sq_king[side_king]])
			| ((pos.pieces[ROOKS] | pos.pieces[QUEENS]) & slide_map[ROOK][pos.sq_king[side_king]]))
	};

	while (all_attacker)
	{
		// generating rays centered on the king square

		auto rays_attacker{ by_slider<QUEEN>(pos.sq_king[side_king], all_attacker) };
		auto attacker{ 1ULL << bit::scan(all_attacker) };
		if (!(attacker & rays_attacker))
		{
			all_attacker &= all_attacker - 1;
			continue;
		}

		// creating final ray from king to attacker

		uint64 xray{};
		for (auto &ray : magic::ray)
		{
			auto flood{ king_sq_bit };
			for ( ; !(flood & ray.boarder); flood |= bit::shift(flood, ray.shift));
			if (flood & attacker)
			{
				xray = flood & rays_attacker;
				break;
			}
		}

		assert(xray & attacker);
		assert(!(xray & king_sq_bit));

		// allowing only moves inside the x-ray between attacker and king

		auto xray_between{ xray ^ attacker };
		if (bit::popcnt(xray_between & pos.side[BOTH]) == 1 && (xray_between & pos.side[side_pin]))
		{
			assert(bit::popcnt(xray_between & pos.side[side_pin]) == 1);
			auto sq{ bit::scan(xray_between & pos.side[side_pin]) };
			pin_moves[sq] = ~xray;
		}

		// if two pawns from different sides are in the x-ray between an attacking rook/queen and king,
		// capturing each other en-passant has to be prohibited

		else if (pos.ep_rear
			&& (xray_between & pos.side[side_king]  & pos.pieces[PAWNS])
			&& (xray_between & pos.side[side_enemy] & pos.pieces[PAWNS])
			&& bit::popcnt(xray_between & pos.side[BOTH]) == 2)
		{
			assert(bit::popcnt(xray & pos.side[side_enemy]) == 2);

			auto target  { xray & pos.side[side_pin ^ 1] & pos.pieces[PAWNS] };
			auto attacker{ xray & pos.side[side_pin] & pos.pieces[PAWNS] };

			if ((attacker << 1 == target || attacker >> 1 == target)
				&& (pos.ep_rear == bit::shift(target, shift::push[side_pin])))
			{
				auto sq{ bit::scan(xray_between & pos.side[side_pin]) };
					pin_moves[sq] = pos.ep_rear;
			}
		}
		all_attacker &= all_attacker - 1;
	}
}

uint64 attack::by_piece(int piece, int sq, int side, const uint64 &occ)
{
	// returning all squares the <piece> of <side> on square <sq> attacks
	 
	switch (piece)
	{
	case PAWNS:   return king_map[sq] & slide_map[BISHOP][sq] & in_front[side][sq];
	case KNIGHTS: return knight_map[sq];
	case BISHOPS: return by_slider<BISHOP>(sq, occ);
	case ROOKS:   return by_slider<ROOK>(sq, occ);
	case QUEENS:  return by_slider<QUEEN>(sq, occ);
	case KINGS:   return king_map[sq];
	default:      assert(false); return 0ULL;
	}
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

	// if available using the faster BMI2 PEXT instruction instead

#if defined(PEXT)
	return magic::attack_table[_pext_u64(occ, magic::slider[sl][sq].mask) + magic::slider[sl][sq].offset];

#else
	occ  &= magic::slider[sl][sq].mask;
	occ  *= magic::slider[sl][sq].magic;
	occ >>= magic::slider[sl][sq].shift;
	occ  += magic::slider[sl][sq].offset;
	return  magic::attack_table[occ];

#endif
}

uint64 attack::by_pawns(const board &pos, int side)
{
	// returning all squares attacked by pawns of <side>

	assert(side == WHITE || side == BLACK);

	return bit::shift(pos.pieces[PAWNS] & pos.side[side] & ~bit::file_west[side], shift::capture_west[side])
		 | bit::shift(pos.pieces[PAWNS] & pos.side[side] & ~bit::file_east[side], shift::capture_east[side]);
}

int attack::see(const board &pos, uint32 move)
{
	// doing a static exchange evaluation of the square reached by <move>

	move::elements el{ move::decode(move) };

	assert(el.piece == pos.piece[el.sq1]);
	assert(el.turn  == pos.turn);

	if (move::castling(el.flag))
		el.sq2 = square::king_target[el.turn][move::castle_side(el.flag)];

	auto attacker{ 1ULL << el.sq1 };
	auto occ{ pos.side[BOTH] };
	auto set{ attack::to_square(pos, el.sq2, occ) };
	auto xray_block{ occ ^ pos.pieces[KINGS] ^ pos.pieces[KNIGHTS] };

	int gain[32]{ value[el.victim] };
	int depth{};

	if (el.flag >= PROMO_ALL)
		el.piece = move::promo_piece(el.flag);

	if (el.flag == ENPASSANT)
	{
		assert(pos.piece[bit::scan(bit::shift(pos.ep_rear, shift::push[pos.xturn]))] == PAWNS);
		attacker |= bit::shift(pos.ep_rear, shift::push[pos.xturn]);
	}

	// looping through the exchange sequence

	for ( ; attacker; attacker = least_valuable(pos, set & pos.side[el.turn], el.piece))
	{
		depth += 1;
		gain[depth] = value[el.piece] - gain[depth - 1];

		// pruning early if the exchange balance is decisive (but the resulting score may not be exact)

		if (std::max(-gain[depth - 1], gain[depth]) < 0)
			break;

		el.turn ^= 1;
		set ^= attacker;
		occ ^= attacker;

		// adding uncovered x-ray attackers to the set

		if (attacker & xray_block)
			set |= add_xray_attacker(pos, el.sq2, occ);
	}
	return sum_gain(gain, depth);
}