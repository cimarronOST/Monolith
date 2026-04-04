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


#include <array>
#include <vector>
#include <initializer_list>
#include <algorithm>

#include "main.h"
#include "types.h"
#include "move.h"
#include "attack.h"
#include "bit.h"
#include "movegen.h"

template class gen<mode::LEGAL>;
template class gen<mode::PSEUDOLEGAL>;

namespace pawn_gen
{
	// pawn move generation details

	struct gen
	{
		stage st{};
		std::array<bit64, 2> mask{};
		std::array<int, 2> shift{};
		flag fl{};
	};

	constexpr std::array<std::array<gen, 2>, 7> gen_detail
	{ {
		{{
			{ stage::QUIET, {{ bit::max, bit::max }}, shift::push1x, NO_FLAG },
            { stage::QUIET, {{ bit::rank[RANK_2], bit::rank[RANK_7] }}, shift::push2x, NO_FLAG }
		}},
		{{
            { stage::QUIET_PROMO_ALL, {{ bit::max, bit::max }}, shift::push1x, PROMO_QUEEN },
            { stage::QUIET_PROMO_ALL, {{ 0ULL, 0ULL }}, {{ 0, 0 }}, NO_FLAG }
		}},
		{{
            { stage::QUIET_PROMO_QUEEN, {{ bit::max, bit::max }}, shift::push1x, PROMO_QUEEN },
            { stage::QUIET_PROMO_QUEEN, {{ 0ULL, 0ULL }}, {{ 0, 0 }}, NO_FLAG }
		}},
		{{
            { stage::CAPTURE, bit::no_west_boarder, shift::capture_west, NO_FLAG },
            { stage::CAPTURE, bit::no_east_boarder, shift::capture_east, NO_FLAG }
		}},
		{{
            { stage::CAPTURE_PROMO_ALL, bit::no_west_boarder, shift::capture_west, PROMO_QUEEN },
            { stage::CAPTURE_PROMO_ALL, bit::no_east_boarder, shift::capture_east, PROMO_QUEEN }
		}},
		{{
            { stage::CAPTURE_PROMO_QUEEN, bit::no_west_boarder, shift::capture_west, PROMO_QUEEN },
            { stage::CAPTURE_PROMO_QUEEN, bit::no_east_boarder, shift::capture_east, PROMO_QUEEN }
		}},
		{{
            { stage::ENPASSANT, bit::no_west_boarder, shift::capture_west, ENPASSANT },
            { stage::ENPASSANT, bit::no_east_boarder, shift::capture_east, ENPASSANT }
		}}
	} };
}

template int gen<mode::LEGAL>::gen_all();
template int gen<mode::PSEUDOLEGAL>::gen_all();
template<mode md> int gen<md>::gen_all()
{
	// generating all possible moves

	verify(cnt.mv == 0);
    pawns<stage::QUIET>();
    pawns<stage::CAPTURE>();
    pawns<stage::ENPASSANT>();
    pawns<stage::QUIET_PROMO_ALL>();
    pawns<stage::CAPTURE_PROMO_ALL>();

    pieces<stage::CAPTURE>({ KNIGHT, BISHOP, ROOK, QUEEN, KING });
    pieces<stage::QUIET>  ({ KNIGHT, BISHOP, ROOK, QUEEN, KING });
	castle();

	return cnt.mv;
}

template int gen<mode::LEGAL>::gen_searchmoves(const std::vector<move>&);
template int gen<mode::PSEUDOLEGAL>::gen_searchmoves(const std::vector<move>&);
template<mode md> int gen<md>::gen_searchmoves(const std::vector<move>& moves)
{
	// reacting to the UCI 'go searchmoves' command

	for (cnt.mv = 0; cnt.mv < (int)moves.size(); ++cnt.mv)
		mv[cnt.mv] = moves[cnt.mv];
	return cnt.mv;
}

// external move generating functions called by movepick

template int gen<mode::LEGAL>::gen_hash(move&);
template int gen<mode::PSEUDOLEGAL>::gen_hash(move&);
template<mode md> int gen<md>::gen_hash(move& hash_mv)
{
	// "generating" the hash move

	verify(cnt.mv == 0);
	if (hash_mv)
	{
		if (pos.pseudolegal(hash_mv))
			mv[cnt.mv++] = hash_mv;
		else
			hash_mv = move{};
	}
	return cnt.mv;
}

template int gen<mode::LEGAL>::gen_capture();
template int gen<mode::PSEUDOLEGAL>::gen_capture();
template<mode md> int gen<md>::gen_capture()
{
	// generating all capturing moves

	verify(cnt.mv == 0);
    pawns <stage::CAPTURE>  ();
    pawns <stage::ENPASSANT>();
    pieces<stage::CAPTURE>  ({ KING, QUEEN, ROOK, BISHOP, KNIGHT });

	cnt.capture = cnt.mv;
	return cnt.mv;
}

template int gen<mode::LEGAL>::gen_promo(stage);
template int gen<mode::PSEUDOLEGAL>::gen_promo(stage);
template<mode md> int gen<md>::gen_promo(stage st)
{
	// generating all promotion moves
	// capture-generation always has to take place before to get counters right

    if (st == stage::PROMO_ALL)
	{
        pawns<stage::CAPTURE_PROMO_ALL>();
        pawns<stage::QUIET_PROMO_ALL>();
	}
	else if (st == stage::PROMO_QUEEN)
	{
        pawns<stage::CAPTURE_PROMO_QUEEN>();
        pawns<stage::QUIET_PROMO_QUEEN>();
	}

	cnt.promo = cnt.mv - cnt.capture;
	return cnt.promo;
}

template int gen<mode::LEGAL>::gen_killer(const killer_list&, move);
template int gen<mode::PSEUDOLEGAL>::gen_killer(const killer_list&, move);
template<mode md> int gen<md>::gen_killer(const killer_list& killer, move counter)
{
	// "generating" quiet killer moves

	verify(cnt.mv == 0);
	for (move kill : killer.mv)
	{
		if (pos.pseudolegal(kill))
			mv[cnt.mv++] =  kill;
	}

	// "generating" the counter move

	if (pos.pseudolegal(counter) && counter != killer.mv[0] && counter != killer.mv[1])
		mv[cnt.mv++] = counter;

	return cnt.mv;
}

template int gen<mode::LEGAL>::gen_quiet();
template int gen<mode::PSEUDOLEGAL>::gen_quiet();
template<mode md> int gen<md>::gen_quiet()
{
	// generating all quiet moves

    pieces<stage::QUIET>({ KING, QUEEN, ROOK, BISHOP, KNIGHT });
	castle();
	pawns <stage::QUIET>();

	return cnt.mv;
}

template int gen<mode::LEGAL>::restore_loosing();
template int gen<mode::PSEUDOLEGAL>::restore_loosing();
template<mode md> int gen<md>::restore_loosing()
{
	// restoring loosing captures (copying them back from the bottom of the movelist)

	for (int i{}; i < cnt.loosing; ++i)
		mv[i] = mv[lim::moves - 1 - i];

	verify(cnt.mv == 0);
	verify(cnt.capture == 0);
	verify(cnt.promo == 0);

	cnt.mv = cnt.loosing;
	return cnt.mv;
}

template<mode md> template<stage st> bit64 gen<md>::pawn_mask()
{
	// generating the appropriate mask depending on the type of pawn move

	if constexpr (st == stage::QUIET)
		return evasions & ~bit::rank_promo & ~pos.side[BOTH];
	if constexpr (st == stage::QUIET_PROMO_ALL)
		return evasions & bit::rank_promo & ~pos.side[BOTH];
	if constexpr (st == stage::QUIET_PROMO_QUEEN)
		return evasions & bit::rank_promo & ~pos.side[BOTH];
	if constexpr (st == stage::CAPTURE)
		return evasions & ~bit::rank_promo & pos.side[pos.cl_x];
	if constexpr (st == stage::CAPTURE_PROMO_ALL)
		return evasions & bit::rank_promo & pos.side[pos.cl_x];
	if constexpr (st == stage::CAPTURE_PROMO_QUEEN)
		return evasions & bit::rank_promo & pos.side[pos.cl_x];
	if constexpr (st == stage::ENPASSANT)
		return bit::shift(evasions, shift::push1x[pos.cl]) & ~bit::rank_promo & pos.ep_rear;
}

template<mode md> template<stage st> void gen<md>::pawns()
{
	// generating pawn moves

	static_assert(st == pawn_gen::gen_detail[int(st)][0].st);
	bit64 mask{ pawn_mask<st>() };

	for (auto& pawn : pawn_gen::gen_detail[int(st)])
	{
		bit64 targets{ mask & bit::shift(pos.pieces[piece::PAWN] & pos.side[pos.cl] & pawn.mask[pos.cl],
			pawn.shift[pos.cl]) };
    if constexpr (st == stage::QUIET)
			if (pawn.shift == shift::push2x)
				targets &= ~bit::shift(bit::rank_push2x[pos.cl] & pos.side[BOTH], shift::push1x[pos.cl]);

		while (targets)
		{
			square sq2{ bit::scan(targets) };
			square sq1{ sq2 - pawn.shift[0] * (1 - 2 * pos.cl) };
			piece vc{ pos.piece_on[sq2] };
            if constexpr (st == stage::ENPASSANT)
                vc = PAWN;

			if (bit::set(sq2) & ~pin[sq1])
			{
                    if constexpr (st == stage::QUIET_PROMO_ALL || st == stage::CAPTURE_PROMO_ALL)
                        for (flag fl{ pawn.fl }; fl >= PROMO_KNIGHT; fl = fl - 1)
                            mv[cnt.mv++] = move{ sq1, sq2, PAWN, vc, pos.cl, fl };
                    else
                        mv[cnt.mv++] = move{ sq1, sq2, PAWN, vc, pos.cl, pawn.fl };
			}
			targets &= targets - 1;
		}
	}
}

template<mode md> template<stage st> void gen<md>::pieces(std::initializer_list<piece> pc)
{
	// generating piece moves

	bit64 mask{ bit::max };
    if constexpr (st == stage::CAPTURE)
		mask &= pos.side[pos.cl_x];
	if constexpr (st == stage::QUIET)
		mask &= ~pos.side[BOTH];

	for (piece p : pc)
	{
		bit64 pieces{ pos.pieces[p] & pos.side[pos.cl] };
		bit64 ev{ p != KING ? evasions : bit::max };
		while (pieces)
		{
			square sq1{ bit::scan(pieces) };
			bit64 targets{ attack::by_piece(p, sq1, pos.cl, pos.side[BOTH]) & mask & ev & ~pin[sq1] };

            if (p == KING)
				targets = attack::safe(pos, pos.cl, targets);

			while (targets)
			{
				square sq2{ bit::scan(targets) };
                mv[cnt.mv++] = move{ sq1, sq2, p, pos.piece_on[sq2], pos.cl, NO_FLAG };
				targets &= targets - 1;
			}
			pieces &= pieces - 1;
		}
	}
}

template<mode md> void gen<md>::castle()
{
	// generating castling moves

    for (flag fl : { CASTLE_EAST, CASTLE_WEST })
	{
        if (pos.castle_right[pos.cl][fl] == NO_SQUARE)
			continue;

		square king_sq1{ pos.sq_king[pos.cl] };
		square king_sq2{ move::king_target[pos.cl][fl] };
		square rook_sq1{ pos.castle_right[pos.cl][fl] };
		square rook_sq2{ move::rook_target[pos.cl][fl] };
		square sq_max{}, sq_min{};

        if (fl == CASTLE_EAST)
		{
			sq_max = std::max(king_sq1, rook_sq2);
			sq_min = std::min(king_sq2, rook_sq1);
		}
		else
		{
            sq_max = std::max(king_sq2, rook_sq1);
            sq_min = std::min(king_sq1, rook_sq2);
		}

		bit64 occ{ pos.side[BOTH] ^ (bit::set(king_sq1) | bit::set(rook_sq1)) };
		if (!(bit::between[sq_max][sq_min] & occ))
		{
			// castling generation is always legal

			bool no_check{ true };
			bit64 king_path{ bit::between[king_sq1][king_sq2] };
			while (king_path)
			{
				square sq{ bit::scan(king_path) };
				if (attack::sq(pos, sq, occ) & pos.side[pos.cl_x])
				{
					no_check = false;
					break;
				}
				king_path &= king_path - 1;
			}
            if (no_check)
                mv[cnt.mv++] = move{ king_sq1, rook_sq1, KING, NO_PIECE, pos.cl, fl };
		}
	}
}
