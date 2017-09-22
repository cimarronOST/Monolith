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


#include <algorithm>

#include "attack.h"
#include "bitboard.h"
#include "magic.h"
#include "movegen.h"

uint64 movegen::slide_ray[2][64];
uint64 movegen::knight_table[64];
uint64 movegen::king_table[64];

namespace
{
	inline void real_shift(uint64 &bb, int shift)
	{
		bb = (bb << shift) | (bb >> (64 - shift));
	}

	static_assert(CAPTURE == 0, "index");
	static_assert(QUIET   == 1, "index");

	// bitboards for pawn move generation

	const int cap_left[]{ 9, 55, -9 };
	const int cap_right[]{ 7, 57, -7 };
	const int push[]{ 8, 56, -8 };
	const int double_push[]{ 16, 48, -16 };

	const uint64 promo_rank{ rank[R1] | rank[R8] };

	const uint64 third_rank[]{ rank[R3], rank[R6] };
	const uint64 boarder[]{ file[A], file[H] };
}

// move generating functions

void movegen::init_list()
{
	// initialising pawn bitboards

	AHEAD = board.turn;
	BACK  = board.turn << 1;

	not_right = ~boarder[board.not_turn];
	not_left  = ~boarder[board.turn];
	pawn_rank = third_rank[board.turn];

	// initialising all other bitboards

	friends = board.side[board.turn];
	enemies = board.side[board.not_turn];
	fr_king = board.pieces[KINGS] & friends;

	gentype[CAPTURE] = enemies;
	gentype[QUIET] = ~board.side[BOTH];

	if (mode == LEGAL)
	{
		find_pins();
		find_evasions();
	}
	else
	{
		assert(mode == PSEUDO);
		evasions = ~0ULL;
	}
}

int movegen::gen_hash()
{
	// "generating" the hash move (that was actually passed to the constructor)

	assert(cnt.moves == 0);
	assert(cnt.hash == 0);

	if (hash_move != NO_MOVE && is_pseudolegal(hash_move))
	{
		movelist[0] = hash_move;
		cnt.moves = cnt.hash = 1;
	}

	return cnt.moves;
}

int movegen::gen_tactical()
{
	assert(cnt.moves == 0);
	assert(cnt.capture == 0);

	// generating captures

	king(CAPTURE);
	pawn_capt();
	queen(CAPTURE);
	rook(CAPTURE);
	bishop(CAPTURE);
	knight(CAPTURE);

	cnt.capture = cnt.moves;

	// generating promotions

	pawn_promo();

	cnt.promo = cnt.moves - cnt.capture;
	return cnt.moves;
}

int movegen::gen_quiet()
{
	// generating quiet moves

	assert(cnt.capture + cnt.promo == cnt.moves);
	assert(cnt.quiet == 0);

	king(QUIET);
	queen(QUIET);
	rook(QUIET);
	bishop(QUIET);
	knight(QUIET);
	pawn_quiet();

	cnt.quiet = cnt.moves - cnt.capture - cnt.promo;
	return cnt.quiet;
}

int movegen::gen_loosing()
{
	// "generating" loosing captures (copying them back)

	for (int i{ 0 }; i < cnt.loosing; ++i)
	{
		movelist[i] = movelist[lim::movegen - 1 - i];
	}

	assert(cnt.moves == 0);
	assert(cnt.quiet == 0);
	assert(cnt.capture == 0);
	assert(cnt.hash == 0);
	assert(cnt.promo == 0);

	cnt.moves = cnt.loosing;
	return cnt.moves;
}

void movegen::pawn_promo()
{
	// capturing left

	uint64 targets{ shift(board.pieces[PAWNS] & friends & not_left, cap_left[AHEAD]) & evasions & promo_rank & enemies };
	while (targets)
	{
		auto target{ bb::bitscan(targets) };
		auto origin{ target - cap_left[BACK] };

		assert((1ULL << origin) & board.pieces[PAWNS] & friends);

		if ((1ULL << target) & ~pin[origin])
		{
			for (int flag{ 15 }; flag >= 12; --flag)
				movelist[cnt.moves++] = encode(origin, target, flag, PAWNS, board.piece_sq[target], board.turn);
		}

		targets &= targets - 1;
	}

	// capturing right

	targets = shift(board.pieces[PAWNS] & friends & not_right, cap_right[AHEAD]) & evasions & promo_rank & enemies;
	while (targets)
	{
		auto target{ bb::bitscan(targets) };
		auto origin{ target - cap_right[BACK] };

		assert((1ULL << origin) & board.pieces[PAWNS] & friends);

		if ((1ULL << target) & ~pin[origin])
		{
			for (int flag{ 15 }; flag >= 12; --flag)
				movelist[cnt.moves++] = encode(origin, target, flag, PAWNS, board.piece_sq[target], board.turn);
		}

		targets &= targets - 1;
	}

	// single push to promotion

	targets = shift(board.pieces[PAWNS] & friends, push[AHEAD]) & ~board.side[BOTH] & evasions & promo_rank;
	while (targets)
	{
		auto target{ bb::bitscan(targets) };
		auto origin{ target - push[BACK] };

		assert((1ULL << origin) & board.pieces[PAWNS] & friends);

		if ((1ULL << target) & ~pin[origin])
		{
			for (int flag{ 15 }; flag >= 12; --flag)
				movelist[cnt.moves++] = encode(origin, target, flag, PAWNS, NONE, board.turn);
		}

		targets &= targets - 1;
	}
}

void movegen::pawn_capt()
{
	// capturing left

	uint64 targets{ shift(board.pieces[PAWNS] & friends & not_left, cap_left[AHEAD]) & ~promo_rank };
	uint64 targets_cap{ targets & enemies & evasions };

	while (targets_cap)
	{
		auto target{ bb::bitscan(targets_cap) };
		auto origin{ target - cap_left[BACK] };

		assert((1ULL << origin) & board.pieces[PAWNS] & friends);

		if ((1ULL << target) & ~pin[origin])
			movelist[cnt.moves++] = encode(origin, target, NONE, PAWNS, board.piece_sq[target], board.turn);

		targets_cap &= targets_cap - 1;
	}

	// enpassant left

	uint64 target_ep{ targets & board.ep_sq & shift(evasions, push[AHEAD]) };
	if (target_ep)
	{
		auto target{ bb::bitscan(target_ep) };
		auto origin{ target - cap_left[BACK] };

		assert((1ULL << origin) & board.pieces[PAWNS] & friends);

		target_ep &= ~pin[origin];
		if (target_ep)
			movelist[cnt.moves++] = encode(origin, target, ENPASSANT, PAWNS, PAWNS, board.turn);
	}

	// capturing right

	targets = shift(board.pieces[PAWNS] & friends & not_right, cap_right[AHEAD]) & ~promo_rank;
	targets_cap = targets & enemies & evasions;

	while (targets_cap)
	{
		auto target{ bb::bitscan(targets_cap) };
		auto origin{ target - cap_right[BACK] };

		assert((1ULL << origin) & board.pieces[PAWNS] & friends);

		if ((1ULL << target) & ~pin[origin])
			movelist[cnt.moves++] = encode(origin, target, NONE, PAWNS, board.piece_sq[target], board.turn);

		targets_cap &= targets_cap - 1;
	}

	// enpassant right

	target_ep = targets & board.ep_sq & shift(evasions, push[AHEAD]);
	if (target_ep)
	{
		auto target{ bb::bitscan(target_ep) };
		auto origin{ target - cap_right[BACK] };

		assert((1ULL << origin) & board.pieces[PAWNS] & friends);

		target_ep &= ~pin[origin];
		if (target_ep)
			movelist[cnt.moves++] = encode(origin, target, ENPASSANT, PAWNS, PAWNS, board.turn);
	}
}

void movegen::pawn_quiet()
{
	// single push

	uint64 pushes{ shift(board.pieces[PAWNS] & friends, push[AHEAD]) & ~board.side[BOTH] & ~promo_rank };
	uint64 targets{ pushes & evasions };

	while (targets)
	{
		auto target{ bb::bitscan(targets) };
		auto origin{ target - push[BACK] };

		assert((1ULL << origin) & board.pieces[PAWNS] & friends);

		if ((1ULL << target) & ~pin[origin])
			movelist[cnt.moves++] = encode(origin, target, NONE, PAWNS, NONE, board.turn);

		targets &= targets - 1;
	}

	// double push

	uint64 targets2x{ shift(pushes & pawn_rank, push[AHEAD]) & evasions & ~board.side[BOTH] };
	while (targets2x)
	{
		auto target{ bb::bitscan(targets2x) };
		auto origin{ target - double_push[BACK] };

		assert((1ULL << origin) & board.pieces[PAWNS] & friends);

		if ((1ULL << target) & ~pin[origin])
			movelist[cnt.moves++] = encode(origin, target, DOUBLEPUSH, PAWNS, NONE, board.turn);

		targets2x &= targets2x - 1;
	}
}

void movegen::knight(GEN_STAGE stage)
{
	uint64 pieces{ board.pieces[KNIGHTS] & friends };
	while (pieces)
	{
		auto sq1{ bb::bitscan(pieces) };
		uint64 targets{ knight_table[sq1] & gentype[stage] & evasions & ~pin[sq1] };

		while (targets)
		{
			auto sq2{ bb::bitscan(targets) };
			movelist[cnt.moves++] = encode(sq1, sq2, NONE, KNIGHTS, board.piece_sq[sq2], board.turn);
			targets &= targets - 1;
		}
		pieces &= pieces - 1;
	}
}

void movegen::bishop(GEN_STAGE stage)
{
	uint64 pieces{ board.pieces[BISHOPS] & friends };
	while (pieces)
	{
		auto sq1{ bb::bitscan(pieces) };
		uint64 targets{ attack::by_slider(BISHOP, sq1, board.side[BOTH]) & gentype[stage] & evasions & ~pin[sq1] };

		while (targets)
		{
			auto sq2{ bb::bitscan(targets) };
			movelist[cnt.moves++] = encode(sq1, sq2, NONE, BISHOPS, board.piece_sq[sq2], board.turn);
			targets &= targets - 1;
		}
		pieces &= pieces - 1;
	}
}

void movegen::rook(GEN_STAGE stage)
{
	uint64 pieces{ board.pieces[ROOKS] & friends };
	while (pieces)
	{
		auto sq1{ bb::bitscan(pieces) };
		uint64 targets{ attack::by_slider(ROOK, sq1, board.side[BOTH]) & gentype[stage] & evasions & ~pin[sq1] };

		while (targets)
		{
			auto sq2{ bb::bitscan(targets) };
			movelist[cnt.moves++] = encode(sq1, sq2, NONE, ROOKS, board.piece_sq[sq2], board.turn);
			targets &= targets - 1;
		}
		pieces &= pieces - 1;
	}
}

void movegen::queen(GEN_STAGE stage)
{
	uint64 pieces{ board.pieces[QUEENS] & friends };
	while (pieces)
	{
		auto sq1{ bb::bitscan(pieces) };

		uint64 targets{ attack::by_slider(BISHOP, sq1, board.side[BOTH]) | attack::by_slider(ROOK, sq1, board.side[BOTH]) };
		targets &= gentype[stage] & evasions & ~pin[sq1];

		while (targets)
		{
			auto sq2{ bb::bitscan(targets) };
			movelist[cnt.moves++] = encode(sq1, sq2, NONE, QUEENS, board.piece_sq[sq2], board.turn);
			targets &= targets - 1;
		}
		pieces &= pieces - 1;
	}
}

void movegen::king(GEN_STAGE stage)
{
	// normal king moves

	uint64 targets{ attack::check(board, board.turn, king_table[board.king_sq[board.turn]] & gentype[stage]) };
	while (targets)
	{
		auto sq{ bb::bitscan(targets) };
		movelist[cnt.moves++] = encode(board.king_sq[board.turn], sq, NONE, KINGS, board.piece_sq[sq], board.turn);
		targets &= targets - 1;
	}

	// castling moves

	if (stage == QUIET && fr_king & 0x800000000000008)
	{
		const uint64 rank_king{ rank[board.turn * 7] };

		if (board.castl_rights[board.turn]
			&& !(board.side[BOTH] & 0x0600000000000006 & rank_king)
			&& bb::popcnt(attack::check(board, board.turn, 0x0e0000000000000e & rank_king)) == 3)
		{
			const uint32 target[]{ 1, 57 };
			movelist[cnt.moves++] = encode(board.king_sq[board.turn], target[board.turn], (CASTLING::WHITE_SHORT + board.turn), KINGS, NONE, board.turn);
		}
		if (board.castl_rights[board.turn + 2]
			&& !(board.side[BOTH] & 0x7000000000000070 & rank_king)
			&& bb::popcnt(attack::check(board, board.turn, 0x3800000000000038 & rank_king)) == 3)
		{
			const uint32 target[]{ 5, 61 };
			movelist[cnt.moves++] = encode(board.king_sq[board.turn], target[board.turn], (CASTLING::WHITE_LONG + board.turn), KINGS, NONE, board.turn);
		}
	}
}

// preparing legal move generation

void movegen::find_evasions()
{
	const uint64 in_front[]{ ~(fr_king - 1), fr_king - 1 };
	auto king_sq{ board.king_sq[board.turn] };
	assert(fr_king != 0ULL);

	uint64 att{ attack::by_slider(ROOK, king_sq, board.side[BOTH]) & (board.pieces[ROOKS] | board.pieces[QUEENS]) };
	att |= attack::by_slider(BISHOP, king_sq, board.side[BOTH]) & (board.pieces[BISHOPS] | board.pieces[QUEENS]);
	att |= knight_table[king_sq] & board.pieces[KNIGHTS];
	att |= king_table[king_sq] & board.pieces[PAWNS] & slide_ray[BISHOP][king_sq] & in_front[board.turn];
	att &= enemies;

	int nr_att{ bb::popcnt(att) };

	if (nr_att == 0)
	{
		evasions = ~0ULL;
	}
	else if (nr_att == 1)
	{
		if (att & board.pieces[KNIGHTS] || att & board.pieces[PAWNS])
		{
			evasions = att;
		}
		else
		{
			assert(att & board.pieces[ROOKS] || att & board.pieces[BISHOPS] || att & board.pieces[QUEENS]);
			auto every_att{ attack::by_slider(ROOK, king_sq, board.side[BOTH]) | attack::by_slider(BISHOP, king_sq, board.side[BOTH]) };

			for (int dir{ 0 }; dir < 8; ++dir)
			{
				auto flood{ fr_king };
				for (; !(flood & magic::ray[dir].boarder); flood |= shift(flood, magic::ray[dir].shift)) ;

				if (flood & att)
				{
					evasions = flood & every_att;
					break;
				}
			}
		}
	}
	else
	{
		assert(nr_att == 2);
		evasions = 0ULL;
	}
}

void movegen::find_pins()
{
	auto king_sq{ board.king_sq[board.turn] };

	uint64 att{ slide_ray[ROOK][king_sq] & enemies & (board.pieces[ROOKS] | board.pieces[QUEENS]) };
	att |= slide_ray[BISHOP][king_sq] & enemies & (board.pieces[BISHOPS] | board.pieces[QUEENS]);

	while (att)
	{
		// generating rays centered on the king square

		uint64 ray_to_att{ attack::by_slider(ROOK, king_sq, att) };
		ray_to_att |= attack::by_slider(BISHOP, king_sq, att);

		uint64 attacker{ 1ULL << bb::bitscan(att) };

		if (!(attacker & ray_to_att))
		{
			att &= att - 1;
			continue;
		}

		// creating final ray from king to attacker

		assert(fr_king);

		uint64 x_ray{ 0 };
		for (int dir{ 0 }; dir < 8; ++dir)
		{
			auto flood{ fr_king };
			for (; !(flood & magic::ray[dir].boarder); flood |= shift(flood, magic::ray[dir].shift));

			if (flood & attacker)
			{
				x_ray = flood & ray_to_att;
				break;
			}
		}

		assert(x_ray & attacker);
		assert(!(x_ray & fr_king));

		// pinning all moves

		if ((x_ray & friends) && bb::popcnt(x_ray & board.side[BOTH]) == 2)
		{
			assert(bb::popcnt(x_ray & friends) == 1);

			auto sq{ bb::bitscan(x_ray & friends) };
			pin[sq] = ~x_ray;
		}

		// pinning enpassant captures of pawns

		else if (board.ep_sq
			&& x_ray & friends & board.pieces[PAWNS]
			&& x_ray & enemies & board.pieces[PAWNS]
			&& bb::popcnt(x_ray & board.side[BOTH]) == 3)
		{
			assert(bb::popcnt(x_ray & enemies) == 2);

			uint64 enemy_pawn{ x_ray & enemies & board.pieces[PAWNS] };
			uint64 friend_pawn{ x_ray & friends & board.pieces[PAWNS] };

			if (friend_pawn << 1 == enemy_pawn || friend_pawn >> 1 == enemy_pawn)
			{
				if (board.ep_sq == shift(enemy_pawn, push[board.turn]))
				{
					auto sq{ bb::bitscan(x_ray & friends) };
					pin[sq] = board.ep_sq;
				}
			}
		}

		att &= att - 1;
	}
}

// checking for pseudolegality

bool movegen::is_pseudolegal(uint32 move)
{
	// asserts the correct match between the board and the move

	move_detail md{ decode(move) };

	uint64 sq1_64{ 1ULL << md.sq1 };
	uint64 sq2_64{ 1ULL << md.sq2 };

	assert(md.turn == board.turn);

	if (md.piece != board.piece_sq[md.sq1] || (board.side[md.turn ^ 1] & sq1_64))
		return false;
	if ((md.victim != board.piece_sq[md.sq2] || (board.side[md.turn] & sq2_64)) && md.flag != ENPASSANT)
		return false;

	switch (md.piece)
	{
	case PAWNS:
		if (md.flag == DOUBLEPUSH)
			return board.piece_sq[(md.sq1 + md.sq2) / 2] == NONE;
		else if (md.flag == ENPASSANT)
			return board.ep_sq == sq2_64;
		else
			return true;

	case KNIGHTS:
		return true;

	case BISHOPS:
		return (attack::by_slider(BISHOP, md.sq1, board.side[BOTH]) & sq2_64) != 0ULL;

	case ROOKS:
		return (attack::by_slider(ROOK, md.sq1, board.side[BOTH]) & sq2_64) != 0ULL;

	case QUEENS:
		return ((attack::by_slider(ROOK, md.sq1, board.side[BOTH]) | attack::by_slider(BISHOP, md.sq1, board.side[BOTH])) & sq2_64) != 0ULL;

	case KINGS:
		if (md.flag == NONE)
			return true;
		else
		{
			assert(md.flag >= CASTLING::WHITE_SHORT && md.flag <= CASTLING::BLACK_LONG);

			if (!board.castl_rights[md.flag - 8])
				return false;
			if (bb::popcnt(attack::check(board, md.turn, (1ULL << ((md.sq1 + md.sq2) / 2)) | sq1_64)) != 2)
				return false;

			if (md.flag <= CASTLING::BLACK_SHORT && (board.side[BOTH] & 0x0600000000000006 & rank[md.turn * 7]))
				return false;
			if (md.flag >= CASTLING::WHITE_LONG  && (board.side[BOTH] & 0x7000000000000070 & rank[md.turn * 7]))
				return false;

			return true;
		}

	default:
		assert(false);
		return false;
	}
}

// concerning movelist

uint32 *movegen::find(uint32 move)
{
	return std::find(movelist, movelist + cnt.moves, move);
}

bool movegen::in_list(uint32 move)
{
	return find(move) != movelist + cnt.moves;
}

// bitboard filling functions, called at startup

void movegen::init_ray(int sl)
{
	// generating star rays to find pins and evasions faster

	assert(sl == ROOK || sl == BISHOP);

	for (int sq{ 0 }; sq < 64; ++sq)
	{
		uint64 sq64{ 1ULL << sq };

		for (int dir{ sl }; dir < 8; dir += 2)
		{
			uint64 flood{ sq64 };
			while (!(flood & magic::ray[dir].boarder))
			{
				real_shift(flood, magic::ray[dir].shift);
				slide_ray[sl][sq] |= flood;
			}
		}
	}
}

void movegen::init_king()
{
	// filling king table

	for (int sq{ 0 }; sq < 64; ++sq)
	{
		uint64 sq64{ 1ULL << sq };

		for (int dir{ 0 }; dir < 8; ++dir)
		{
			uint64 att{ sq64 };

			if (!(att & magic::ray[dir].boarder))
				movegen::king_table[sq] |= shift(att, magic::ray[dir].shift);
		}
	}
}

void movegen::init_knight()
{
	// filling knight table

	const magic::pattern jump[]
	{
		{ 15, 0xffff010101010101 },{ 6,  0xff03030303030303 },
		{ 54, 0x03030303030303ff },{ 47, 0x010101010101ffff },
		{ 49, 0x808080808080ffff },{ 58, 0xc0c0c0c0c0c0c0ff },
		{ 10, 0xffc0c0c0c0c0c0c0 },{ 17, 0xffff808080808080 }
	};

	for (int sq{ 0 }; sq < 64; ++sq)
	{
		uint64 sq64{ 1ULL << sq };

		for (int dir{ 0 }; dir < 8; ++dir)
		{
			uint64 att{ sq64 };
			if (!(att & jump[dir].boarder))
				movegen::knight_table[sq] |= shift(att, jump[dir].shift);
		}
	}
}
