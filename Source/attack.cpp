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


#include "magic.h"
#include "bit.h"
#include "attack.h"

#if defined(PEXT)
#include <immintrin.h>
#endif

namespace see
{
	// assisting functions for the Static Exchange Evaluation (SEE)

	std::tuple<bit64, piece> least_valuable(const board &pos, bit64 set)
	{
		// selecting the least valuable attacker of the set

		for (auto& p : pos.pieces)
		{
			if (p & set)
			{
				square sq{ bit::scan(p & set) };
				return std::make_tuple(bit::set(sq), pos.piece_on[sq]);
			}
		}
		return std::make_tuple(0ULL, no_piece);
	}

	bit64 add_xray_attacker(const board& pos, square sq, const bit64& occ)
	{
		// adding uncovered x-ray attackers to the attacker-set

		return occ & ((attack::by_slider<bishop>(sq, occ) & (pos.pieces[bishop] | pos.pieces[queen]))
			        | (attack::by_slider<rook>(sq, occ)   & (pos.pieces[rook]   | pos.pieces[queen])));
	}
}

void attack::pin_mv::find(const board &pos, color cl_king, color cl_piece)
{
	// finding all pieces that are pinned to the king and defining their legal move zone
	// pieces of cl_piece are pinned to the king of cl_king

	color cl_enemy{ cl_king ^ 1 };
	bit64 sq_king_bit{ pos.pieces[king] & pos.side[cl_king] };
	bit64 dia_enemy{ pos.side[cl_enemy] & (pos.pieces[bishop] | pos.pieces[queen]) };
	bit64 lin_enemy{ pos.side[cl_enemy] & (pos.pieces[rook] | pos.pieces[queen]) };

	square sq_king{ pos.sq_king[cl_king] };
	bit64 slider{ (dia_enemy & by_slider<bishop>(sq_king, dia_enemy)) | (lin_enemy & by_slider<rook>(sq_king, lin_enemy)) };

	while (slider)
	{
		// defining the ray between enemy slider and king

		square sq_sl{ bit::scan(slider) };
		bit64 ray{ bit::ray[sq_king][sq_sl] ^ sq_king_bit };
		bit64 piece{ (ray ^ bit::set(sq_sl)) & pos.side[cl_piece] };

		// allowing only moves of the piece inside the ray that stay inside the ray
		// resulting masks get complemented (~) for easier use afterwards

		if (bit::popcnt(ray & pos.side[both]) == 2 && piece)
			add(bit::scan(piece), ~ray);

		// if two pawns of different colors are in the ray between an attacking rook/queen and king,
		// capturing each other en-passant also has to be prohibited

		else if (pos.ep_rear
			&& bit::popcnt(ray & pos.side[both]) == 3
			&& (ray & pos.side[cl_king] & pos.pieces[pawn])
			&& (ray & pos.side[cl_enemy] & pos.pieces[pawn]))
		{
			bit64 pc{ ray & pos.side[cl_piece] & pos.pieces[pawn] };
			bit64 vc{ ray & pos.side[cl_piece ^ 1] & pos.pieces[pawn] };

			if ((pc << 1 == vc || pc >> 1 == vc) && pos.ep_rear == bit::shift(vc, shift::push1x[cl_piece]))
				add(bit::scan(pc), pos.ep_rear);
		}
		slider &= slider - 1;
	}
}

bit64 attack::pin_mv::operator[](square sq) const
{
	// retrieving the move restriction of the pinned piece

	verify(pin[0] == 0ULL || pin[0] == bit::max);
	return pin[pin_lc[sq]];
}

void attack::pin_mv::add(square sq, bit64 bb)
{
	// creating an index for every pin to save memory

	if (pin_lc[sq])
	{
		pin[pin_lc[sq]] |= bb;
	}
	else
	{
		pin_lc[sq] = ++pin_cnt;
		verify(pin_cnt <= 8);
		pin[pin_cnt] = bb;
	}
}

void attack::pin_mv::reset(square sq)
{
	pin_lc[sq] = 0;
}

void attack::pin_mv::set_base(const bit64 &bb)
{
	pin[0] = bb;
}

bit64 attack::check(const board &pos, color cl, bit64 mask)
{
	// returning all squares that are not under enemy attack

	verify(type::cl(cl));

	bit64 occ{ pos.side[both] & ~(pos.pieces[king] & pos.side[cl]) };
	bit64 set{ mask };
	while (set)
	{
		square sq{ bit::scan(set) };
		if (attack::sq(pos, sq, occ) & pos.side[cl ^ 1])
			mask ^= bit::set(sq);
		set &= set - 1;
	}
	return mask;
}

bit64 attack::evasions(const board &pos)
{
	// defining the evasion zone if the king is under attack

	bit64 evasions{};
	square sq{ pos.sq_king[pos.cl] };
	bit64 attacker(attack::sq(pos, sq, pos.side[both]) & pos.side[pos.cl_x]);
	int attacker_cnt{ bit::popcnt(attacker) };

	if (attacker_cnt == 0)
	{
		// with no attacker, no evasion moves are necessary

		evasions = bit::max;
	}
	else if (attacker_cnt == 1)
	{
		// with 1 attacker, only moves that block the attacker's path or capture the attacker are legal

		if (attacker & (pos.pieces[knight] | pos.pieces[pawn]))
			evasions = attacker;
		else
		{
			verify(attacker & (pos.pieces[rook] | pos.pieces[bishop] | pos.pieces[queen]));
			verify(attacker & bit::ray[sq][bit::scan(attacker)]);
			verify(bit::popcnt(bit::ray[sq][bit::scan(attacker)] & pos.side[both]) == 2);
			evasions = bit::ray[sq][bit::scan(attacker)] ^ bit::set(sq);
		}
	}
	else
	{
		// with 2 attackers, only king move evasions may be legal

		verify(attacker_cnt == 2);
		evasions = 0ULL;
	}
	return evasions;
}

bit64 attack::by_piece(piece pc, square sq, color cl, const bit64 &occ)
{
	// returning all squares a piece attacks

	verify(type::pc(pc));
	verify(type::sq(sq));
	verify(type::cl(cl));

	switch (pc)
	{
	case pawn:   return bit::pawn_attack[cl][sq];
	case knight: return bit::pc_attack[knight][sq];
	case bishop: return by_slider<bishop>(sq, occ);
	case rook:   return by_slider<rook>(sq, occ);
	case queen:  return by_slider<queen>(sq, occ);
	case king:   return bit::pc_attack[king][sq];
	default:     verify(false); return 0ULL;
	}
}

template bit64 attack::by_slider<bishop>(square, bit64);
template bit64 attack::by_slider<rook>(square, bit64);
template bit64 attack::by_slider<queen>(square, bit64);

template<piece pc>
bit64 attack::by_slider(square sq, bit64 occ)
{
	// magic index hashing function to generate sliding moves
	// if the BMI2 instruction PEXT is enabled, using PEXT instead for faster performance
	
	if constexpr (pc == queen)
		return by_slider<rook>(sq, occ) | by_slider<bishop>(sq, occ);

	verify(type::sq(sq));
	verify(pc - 2 == magic::bishop || pc - 2 == magic::rook);
	auto& entry{ magic::slider[pc - 2][sq] };

#if defined(PEXT)
	auto index{ entry.offset + _pext_u64(occ, entry.mask) };
#else
	auto index{ entry.offset + ((occ & entry.mask) * entry.magic >> entry.shift) };
#endif
	return magic::attack_table[(uint32)index];
}

bit64 attack::by_pawns(bit64 pawns, color cl)
{
	// returning all squares attacked by pawns

	verify(type::cl(cl));
	return bit::shift(pawns & bit::no_west_boarder[cl], shift::capture_west[cl])
		 | bit::shift(pawns & bit::no_east_boarder[cl], shift::capture_east[cl]);
}

bit64 attack::sq(const board &pos, square sq, const bit64 &occ)
{
	// finding all attacking pieces of a square

	verify(type::sq(sq));
	return (pos.pieces[pawn]   & bit::pawn_attack[white][sq] & pos.side[black])
		|  (pos.pieces[pawn]   & bit::pawn_attack[black][sq] & pos.side[white])
		|  (pos.pieces[knight] & bit::pc_attack[knight][sq])
		| ((pos.pieces[bishop] | pos.pieces[queen]) & by_slider<bishop>(sq, occ))
		| ((pos.pieces[rook]   | pos.pieces[queen]) & by_slider<rook>(sq, occ))
		|  (pos.pieces[king]   & bit::pc_attack[king][sq]);
}

bool attack::see_above(const board& pos, move new_mv, score margin)
{
	// testing a move through Static Exchange Evaluation (SEE)
	// if the evaluation is at least equal to the margin or above, true is returned

	verify_deep(new_mv.sq1() == new_mv.sq2() || pos.pseudolegal(new_mv));

	move::item mv{ new_mv };
	if (mv.castling())
		return 0 >= margin;

	// cutoff if even a free piece is not good enough

	score sc{ value[mv.vc] - margin };
	if (sc < 0)
		return false;

	// cutoff if after the recapture the standing pat is not good enough

	if (mv.fl >= promo_knight)
		mv.pc = mv.promo_pc();
	sc -= value[mv.pc];
	if (sc >= 0)
		return true;

	color cl{ mv.cl ^ 1 };
	bit64 occ{ (pos.side[both] ^ bit::set(mv.sq1)) | bit::set(mv.sq2) };
	if (mv.fl == enpassant)
	{
		verify(bit::shift(pos.ep_rear, shift::push1x[cl]) & pos.pieces[pawn] & pos.side[cl]);
		occ ^= bit::shift(pos.ep_rear, shift::push1x[cl]);
	}
	bit64 attackers{ attack::sq(pos, mv.sq2, occ) & ~bit::set(mv.sq1) };

	while (true)
	{
		auto least{ see::least_valuable(pos, attackers & pos.side[cl]) };
		bit64  att{ std::get<0>(least) };
		piece   pc{ std::get<1>(least) };

		if (!att)
			break;
		attackers ^= att;
		occ ^= att;
		attackers |= see::add_xray_attacker(pos, mv.sq2, occ);
		cl ^= 1;
		
		// minimaxing the running score and adding the next capture

		sc = -sc - score(1) - value[pc];

		// if after the capture the score is still positive, the side to move now looses
		// even if there were further recaptures possible, the material cannot be recovered anymore

		if (sc >= 0)
		{
			// if the king can be recaptured, the capture was illegal and the other side wins

			if (pc == king && (attackers & pos.side[cl]))
				cl ^= 1;
			break;
		}
	}
	return mv.cl != cl;
}

bool attack::escape(const board& pos, move mv)
{
	// checking whether the move escapes a capture

	verify_deep(pos.pseudolegal(mv));
	verify(mv.quiet());

	mv.set_sq2(mv.sq1());
	return !attack::see_above(pos, mv, score(0));
}