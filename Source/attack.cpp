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


#include <tuple>
#include <bit>

#if defined(PEXT)
#include <immintrin.h>
#endif

#include "main.h"
#include "types.h"
#include "board.h"
#include "magic.h"
#include "bit.h"
#include "attack.h"

namespace see
{
	// assisting functions for the Static Exchange Evaluation (SEE)

	static std::tuple<bit64, piece> least_valuable(const board &pos, bit64 set)
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
		return std::make_tuple(0ULL, NO_PIECE);
	}

	static bit64 add_x_ray_attacker(const board& pos, square sq, const bit64& occ)
	{
		// adding uncovered x-ray attackers to the attacker-set

		return occ & ((attack::by_slider<BISHOP>(sq, occ) & (pos.pieces[BISHOP] | pos.pieces[QUEEN]))
			        | (attack::by_slider<ROOK>(sq, occ)   & (pos.pieces[ROOK]   | pos.pieces[QUEEN])));
	}
}

bit64 attack::pin_mv::operator[](square sq) const
{
	// retrieving the move restriction of the pinned piece

	verify(pin[0] == 0ULL || pin[0] == bit::max);
	return pin[pin_lc[sq]];
}

void attack::pin_mv::find(const board &pos, color cl_king, color cl_pc)
{
	// finding all pieces that are pinned to the king and defining their legal move zone
	// pieces of cl_piece are pinned to the king of cl_king

	color cl_enemy{ cl_king ^ 1 };
	bit64 dia_enemy{ pos.side[cl_enemy] & (pos.pieces[BISHOP] | pos.pieces[QUEEN]) };
	bit64 lin_enemy{ pos.side[cl_enemy] & (pos.pieces[ROOK] | pos.pieces[QUEEN]) };

	square sq_king{ pos.sq_king[cl_king] };
	bit64 slider{ (dia_enemy & by_slider<BISHOP>(sq_king, dia_enemy)) | (lin_enemy & by_slider<ROOK>(sq_king, lin_enemy)) };

	while (slider)
	{
		// defining the ray between enemy slider and king

		square sq_sl{ bit::scan(slider) };
		bit64 ray{ bit::ray[sq_king][sq_sl] ^ (pos.pieces[KING] & pos.side[cl_king]) };
		bit64 piece{ (ray ^ bit::set(sq_sl)) & pos.side[cl_pc] };

		// allowing only moves of the piece inside the ray that stay inside the ray
		// resulting masks get complemented (~) for easier use afterwards

		if (std::popcount(ray & pos.side[BOTH]) == 2 && piece)
			add(bit::scan(piece), ~ray);

		// if two pawns of different colors are in the ray between an attacking rook/queen and king,
		// capturing each other en-passant also has to be prohibited

		else if (pos.ep_rear
			&& std::popcount(ray & pos.side[BOTH]) == 3
			&& (ray & pos.side[cl_king] & pos.pieces[PAWN])
			&& (ray & pos.side[cl_enemy] & pos.pieces[PAWN]))
		{
			bit64 pc{ ray & pos.side[cl_pc] & pos.pieces[PAWN] };
			bit64 vc{ ray & pos.side[cl_pc ^ 1] & pos.pieces[PAWN] };

			if ((pc << 1 == vc || pc >> 1 == vc) && pos.ep_rear == bit::shift(vc, shift::push1x[cl_pc]))
				add(bit::scan(pc), pos.ep_rear);
		}
		slider &= slider - 1;
	}
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
		pin_lc[sq] = (int8)++pin_cnt;
		verify(pin_cnt <= 8);
		pin[pin_cnt] = bb;
	}
}

bit64 attack::safe(const board &pos, color cl, bit64 mask)
{
	// returning all squares that are not under enemy attack

	verify(type::cl(cl));

	bit64 occ{ pos.side[BOTH] & ~(pos.pieces[KING] & pos.side[cl]) };
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
	bit64 attacker(attack::sq(pos, sq, pos.side[BOTH]) & pos.side[pos.cl_x]);
	int attacker_cnt{ std::popcount(attacker) };

	if (attacker_cnt == 0)
	{
		// with no attacker, no evasion moves are necessary

		evasions = bit::max;
	}
	else if (attacker_cnt == 1)
	{
		// with 1 attacker, only moves that block the attacker's path or capture the attacker are legal

		if (attacker & (pos.pieces[KNIGHT] | pos.pieces[PAWN]))
			evasions = attacker;
		else
		{
			verify(attacker & (pos.pieces[ROOK] | pos.pieces[BISHOP] | pos.pieces[QUEEN]));
			verify(attacker & bit::ray[sq][bit::scan(attacker)]);
			verify(std::popcount(bit::ray[sq][bit::scan(attacker)] & pos.side[BOTH]) == 2);
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
	case PAWN:   return bit::pawn_attack[cl][sq];
	case KNIGHT: return bit::pc_attack[KNIGHT][sq];
	case BISHOP: return by_slider<BISHOP>(sq, occ);
	case ROOK:   return by_slider<ROOK>(sq, occ);
	case QUEEN:  return by_slider<QUEEN>(sq, occ);
	case KING:   return bit::pc_attack[KING][sq];
	default:     verify(false); return 0ULL;
	}
}

template bit64 attack::by_slider<BISHOP>(square, const bit64&);
template bit64 attack::by_slider<ROOK>(square, const bit64&);
template bit64 attack::by_slider<QUEEN>(square, const bit64&);

template<piece pc>
bit64 attack::by_slider(square sq, const bit64& occ)
{
	// magic index hashing function to generate sliding moves
	// if the BMI2 instruction PEXT is enabled, PEXT is used instead for faster performance
	
	if constexpr (pc == QUEEN)
		return by_slider<ROOK>(sq, occ) | by_slider<BISHOP>(sq, occ);

	verify(type::sq(sq));
	verify(pc - 2 == magic::BISHOP || pc - 2 == magic::ROOK);
	auto& entry{ magic::slider[sq][pc - 2] };

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
	return (pos.pieces[PAWN]   & bit::pawn_attack[WHITE][sq] & pos.side[BLACK])
        |  (pos.pieces[PAWN]   & bit::pawn_attack[BLACK][sq] & pos.side[WHITE])
        |  (pos.pieces[PAWN]   & bit::pawn_attack[WHITE][sq] & pos.side[BLACK])
        |  (pos.pieces[KNIGHT] & bit::pc_attack[KNIGHT][sq])
        | ((pos.pieces[BISHOP] | pos.pieces[QUEEN]) & by_slider<BISHOP>(sq, occ))
        | ((pos.pieces[ROOK]   | pos.pieces[QUEEN]) & by_slider<ROOK>(sq, occ))
        |  (pos.pieces[KING]   & bit::pc_attack[KING][sq]);
}

bool attack::see_above(const board& pos, move new_mv, score margin)
{
	// testing a move through Static Exchange Evaluation (SEE)
	// if the evaluation is at least equal to the margin or above, true is returned

	verify(new_mv.sq1() == new_mv.sq2() || pos.pseudolegal(new_mv));

	move::item mv{ new_mv };
	if (mv.castling())
		return 0 >= margin;

	// cutoff if even a free piece is not good enough

	score sc{ value[mv.vc] - margin };
	if (sc < 0)
		return false;

	// cutoff if after the recapture the standing pat is not good enough

	if (mv.fl >= PROMO_KNIGHT)
		mv.pc = mv.promo_pc();
	sc -= value[mv.pc];
	if (sc >= 0)
		return true;

	color cl{ mv.cl ^ 1 };
	bit64 occ{ (pos.side[BOTH] ^ bit::set(mv.sq1)) | bit::set(mv.sq2) };
	if (mv.fl == ENPASSANT)
	{
		verify(bit::shift(pos.ep_rear, shift::push1x[cl]) & pos.pieces[PAWN] & pos.side[cl]);
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
		attackers |= see::add_x_ray_attacker(pos, mv.sq2, occ);
		cl ^= 1;
		
		// minimaxing the running score and adding the next capture

		sc = -sc - score(1) - value[pc];

		// if after the capture the score is still positive, the side to move now looses
		// even if there were further recaptures possible, the material cannot be recovered anymore

		if (sc >= 0)
		{
			// if the king can be recaptured, the capture was illegal and the other side wins

			if (pc == KING && (attackers & pos.side[cl]))
				cl ^= 1;
			break;
		}
	}
	return mv.cl != cl;
}

bool attack::escape(const board& pos, move mv)
{
	// checking whether the move escapes a capture

	verify(pos.pseudolegal(mv));
	verify(mv.quiet());
	mv.set_sq2(mv.sq1());
	return !attack::see_above(pos, mv, score(0));
}