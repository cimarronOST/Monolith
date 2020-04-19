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


#include "attack.h"
#include "bit.h"
#include "movegen.h"

template class gen<mode::legal>;
template class gen<mode::pseudolegal>;

namespace pawn_gen
{
	// pawn move generation details

	struct gen
	{
		std::array<bit64, 2> mask{};
		std::array<int, 2> shift{};
		flag fl{};
	};

	constexpr std::array<std::array<gen, 2>, 7> gen_detail
	{ {
		{{	// quiet
			{{{ bit::max, bit::max }}, shift::push1x, no_flag },
			{{{ bit::rank[rank_2], bit::rank[rank_7] }}, shift::push2x, no_flag }
		}},
		{{  // quiet_promo_all
			{{{ bit::max, bit::max }}, shift::push1x, promo_queen },
			{{{ 0ULL, 0ULL }}, {{ 0, 0 }}, no_flag }
		}},
		{{  // quiet_promo_queen
			{{{ bit::max, bit::max }}, shift::push1x, promo_queen },
			{{{ 0ULL, 0ULL }}, {{ 0, 0 }}, no_flag }
		}},
		{{  // capture
			{ bit::no_west_boarder, shift::capture_west, no_flag },
			{ bit::no_east_boarder, shift::capture_east, no_flag }
			}},
		{{  // capture_promo_all
			{ bit::no_west_boarder, shift::capture_west, promo_queen },
			{ bit::no_east_boarder, shift::capture_east, promo_queen }
		}},
		{{  // capture_promo_queen
			{ bit::no_west_boarder, shift::capture_west, promo_queen },
			{ bit::no_east_boarder, shift::capture_east, promo_queen }
		}},
		{{  // enpassant
			{ bit::no_west_boarder, shift::capture_west, enpassant },
			{ bit::no_east_boarder, shift::capture_east, enpassant }
		}}
	} };
}

template int gen<mode::legal>::gen_all();
template int gen<mode::pseudolegal>::gen_all();

template<mode md> int gen<md>::gen_all()
{
	// generating all possible moves

	verify(cnt.mv == 0);
	pawns<stage::quiet>();
	pawns<stage::capture>();
	pawns<stage::enpassant>();
	pawns<stage::quiet_promo_all>();
	pawns<stage::capture_promo_all>();

	pieces<stage::capture>({ knight, bishop, rook, queen, king });
	pieces<stage::quiet>  ({ knight, bishop, rook, queen, king });
	castle();

	return cnt.mv;
}

template int gen<mode::legal>::gen_searchmoves(const std::vector<move>&);
template int gen<mode::pseudolegal>::gen_searchmoves(const std::vector<move>&);

template<mode md> int gen<md>::gen_searchmoves(const std::vector<move>& moves)
{
	// reacting to the UCI 'go searchmoves' command

	for (cnt.mv = 0; cnt.mv < (int)moves.size(); ++cnt.mv)
		mv[cnt.mv] = moves[cnt.mv];
	return cnt.mv;
}

// external move generating functions called by movepick

template int gen<mode::legal>::gen_hash(move&);
template int gen<mode::pseudolegal>::gen_hash(move&);

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

template int gen<mode::legal>::gen_capture();
template int gen<mode::pseudolegal>::gen_capture();

template<mode md> int gen<md>::gen_capture()
{
	// generating all capturing moves

	verify(cnt.mv == 0);
	pawns <stage::capture>  ();
	pawns <stage::enpassant>();
	pieces<stage::capture>  ({ king, queen, rook, bishop, knight });

	cnt.capture = cnt.mv;
	return cnt.mv;
}

template int gen<mode::legal>::gen_promo(stage);
template int gen<mode::pseudolegal>::gen_promo(stage);

template<mode md> int gen<md>::gen_promo(stage st)
{
	// generating all promotion moves
	// capture-generation always has to take place before to get counters right

	if (st == stage::promo_all)
	{
		pawns<stage::capture_promo_all>();
		pawns<stage::quiet_promo_all>();
	}
	else if (st == stage::promo_queen)
	{
		pawns<stage::capture_promo_queen>();
		pawns<stage::quiet_promo_queen>();
	}

	cnt.promo = cnt.mv - cnt.capture;
	return cnt.promo;
}

template int gen<mode::legal>::gen_killer(const killer_list&, move);
template int gen<mode::pseudolegal>::gen_killer(const killer_list&, move);

template<mode md> int gen<md>::gen_killer(const killer_list& killer, move counter)
{
	// "generating" quiet killer moves

	verify(cnt.mv == 0);
	for (move kill : killer)
	{
		if (pos.pseudolegal(kill))
			mv[cnt.mv++] =  kill;
	}

	// "generating" the counter move

	if (pos.pseudolegal(counter) && counter != killer[0] && counter != killer[1])
		mv[cnt.mv++] = counter;

	return cnt.mv;
}

template int gen<mode::legal>::gen_quiet();
template int gen<mode::pseudolegal>::gen_quiet();

template<mode md> int gen<md>::gen_quiet()
{
	// generating all quiet moves

	pieces<stage::quiet>({ king, queen, rook, bishop, knight });
	castle();
	pawns <stage::quiet>();

	return cnt.mv;
}

template int gen<mode::legal>::restore_loosing();
template int gen<mode::pseudolegal>::restore_loosing();

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

template int gen<mode::legal>::restore_deferred(const move_list&, int);
template int gen<mode::pseudolegal>::restore_deferred(const move_list&, int);

template<mode md> int gen<md>::restore_deferred(const move_list& deferred, int deferred_cnt)
{
	// restoring deferred moves

	verify(cnt.mv == 0);

	for (; cnt.mv < deferred_cnt; ++cnt.mv)
		mv[cnt.mv] = deferred[cnt.mv];
	return cnt.mv;
}

template<mode md> template<stage st> void gen<md>::pawns()
{
	// generating pawn moves
	// starting with getting stage dependencies right

	static_assert(st != stage::promo_all && st != stage::promo_queen);

	bit64 mask{ bit::max };
	if constexpr (st == stage::quiet)
		mask &= evasions & ~bit::rank_promo & ~pos.side[both];
	if constexpr (st == stage::quiet_promo_all)
		mask &= evasions & bit::rank_promo & ~pos.side[both];
	if constexpr (st == stage::quiet_promo_queen)
		mask &= evasions & bit::rank_promo & ~pos.side[both];
	if constexpr (st == stage::capture)
		mask &= evasions & ~bit::rank_promo & pos.side[pos.cl_x];
	if constexpr (st == stage::capture_promo_all)
		mask &= evasions & bit::rank_promo & pos.side[pos.cl_x];
	if constexpr (st == stage::capture_promo_queen)
		mask &= evasions & bit::rank_promo & pos.side[pos.cl_x];
	if constexpr (st == stage::enpassant)
		mask &= bit::shift(evasions, shift::push1x[pos.cl]) & ~bit::rank_promo & pos.ep_rear;

	// generating the moves

	for (auto& pawn : pawn_gen::gen_detail[int(st)])
	{
		bit64 targets{ mask & bit::shift(pos.pieces[piece::pawn] & pos.side[pos.cl] & pawn.mask[pos.cl],
			pawn.shift[pos.cl]) };
		if constexpr (st == stage::quiet)
			if (pawn.shift == shift::push2x)
				targets &= ~bit::shift(bit::rank_push2x[pos.cl] & pos.side[both], shift::push1x[pos.cl]);

		while (targets)
		{
			square sq2{ bit::scan(targets) };
			square sq1{ sq2 - pawn.shift[0] * (1 - 2 * pos.cl) };
			piece vc{ pos.piece_on[sq2] };
			if constexpr (st == stage::enpassant)
				vc = piece::pawn;

			if (bit::set(sq2) & ~pin[sq1])
			{
				if constexpr (st == stage::quiet_promo_all || st == stage::capture_promo_all)
					for (flag fl{ pawn.fl }; fl >= promo_knight; fl = fl - 1)
						mv[cnt.mv++] = move{ sq1, sq2, piece::pawn, vc, pos.cl, fl };
				else
					mv[cnt.mv++] = move{ sq1, sq2, piece::pawn, vc, pos.cl, pawn.fl };
			}
			targets &= targets - 1;
		}
	}
}

template<mode md> template<stage st> void gen<md>::pieces(std::initializer_list<piece> pc)
{
	// generating piece moves

	bit64 mask{ bit::max };
	if constexpr (st == stage::capture)
		mask &= pos.side[pos.cl_x];
	if constexpr (st == stage::quiet)
		mask &= ~pos.side[both];

	for (piece p : pc)
	{
		bit64 pieces{ pos.pieces[p] & pos.side[pos.cl] };
		bit64 ev{ p != king ? evasions : bit::max };
		while (pieces)
		{
			square sq1{ bit::scan(pieces) };
			bit64 targets{ attack::by_piece(p, sq1, pos.cl, pos.side[both]) & mask & ev & ~pin[sq1] };

			if (p == king)
				targets = attack::check(pos, pos.cl, targets);

			while (targets)
			{
				square sq2{ bit::scan(targets) };
				mv[cnt.mv++] = move{ sq1, sq2, p, pos.piece_on[sq2], pos.cl, no_flag };
				targets &= targets - 1;
			}
			pieces &= pieces - 1;
		}
	}
}

template<mode md> void gen<md>::castle()
{
	// generating castling moves

	for (flag fl : { castle_east, castle_west})
	{
		if (pos.castle_right[pos.cl][fl] == prohibited)
			continue;

		square king_sq1{ pos.sq_king[pos.cl] };
		square king_sq2{ move::king_target[pos.cl][fl] };
		square rook_sq1{ pos.castle_right[pos.cl][fl] };
		square rook_sq2{ move::rook_target[pos.cl][fl] };
		square sq_max{}, sq_min{};

		if (fl == castle_east)
		{
			sq_max = std::max(king_sq1, rook_sq2);
			sq_min = std::min(king_sq2, rook_sq1);
		}
		else
		{
			sq_max = std::max(king_sq2, rook_sq1);
			sq_min = std::min(king_sq1, rook_sq2);
		}

		bit64 occ{ pos.side[both] ^ (bit::set(king_sq1) | bit::set(rook_sq1)) };
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
				mv[cnt.mv++] = move{ king_sq1, rook_sq1, king, no_piece, pos.cl, fl };
		}
	}
}
