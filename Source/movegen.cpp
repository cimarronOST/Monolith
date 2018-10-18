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
#include "move.h"
#include "bit.h"
#include "movegen.h"

bool gen::find(uint32 move)
{
	// checking whether the move can be found in the movelist

	return std::find(this->move, this->move + moves, move) != this->move + moves;
}

bool gen::empty() const
{
	// checking if there are no moves in the movelist

	return moves == 0;
}

void gen::gen_all()
{
	// root node & perft & UCI move generation
	// all moves independent of their stages are generated

	gen_tactical<PROMO_ALL>();
	gen_quiet();
}

void gen::gen_searchmoves(std::vector<uint32> &searchmoves)
{
	// reacting to the UCI 'go searchmoves' command

	std::copy(searchmoves.begin(), searchmoves.end(), move);
	moves = static_cast<int>(searchmoves.size());
}

// external move generating functions called by movepick

int gen::gen_hash(uint32 &hash_move)
{
	// "generating" the hash move

	assert(moves == 0);

	if (hash_move != MOVE_NONE)
	{
		if (pos.pseudolegal(hash_move))
			move[moves++] = hash_move;
		else
			hash_move = MOVE_NONE;
	}
	return moves;
}

template int gen::gen_tactical<PROMO_ALL>();
template int gen::gen_tactical<PROMO_QUEEN>();

template<promo_mode promo>
int gen::gen_tactical()
{
	// generating captures

	assert(moves == 0);
	assert(count.capture == 0);
	auto targets{ pos.side[pos.xturn] };

	king(targets);
	pawn_capture();
	queen(targets);
	rook(targets);
	bishop(targets);
	knight(targets);
	count.capture = moves;

	// generating promotions

	pawn_promo<promo>();
	count.promo = moves - count.capture;

	return moves;
}

int gen::gen_killer(uint32 killer[], uint32 counter)
{
	// "generating" quiet killer moves

	assert(moves == 0);
	for (int slot{}; slot < 2; ++slot)
	{
		if (pos.pseudolegal(killer[slot]))
			move[moves++] = killer[slot];
	}

	// "generating" the counter move

	if (pos.pseudolegal(counter) && counter != killer[0] && counter != killer[1])
		move[moves++] = counter;

	return moves;
}

int gen::gen_quiet()
{
	// generating quiet moves

	auto targets{ ~pos.side[BOTH] };

	king(targets);
	queen(targets);
	rook(targets);
	bishop(targets);
	knight(targets);
	pawn_quiet(targets);

	return moves;
}

int gen::gen_check()
{
	// generating quiet checking moves

	assert(moves == 0);
	assert(count.capture == 0);

	// generating direct checks first, this is done by restricting the target-square-mask
	// to empty squares from which the enemy king can be reached

	auto sq{ pos.sq_king[pos.xturn] };
	auto empty{ ~pos.side[BOTH] };
	auto reach{ empty & attack::by_slider<QUEEN>(sq, pos.side[BOTH]) };
	king_color = bit::color(1ULL << sq);

	queen(reach);
	rook(reach & attack::slide_map[ROOK][sq]);
	bishop(reach & attack::slide_map[BISHOP][sq]);
	knight(empty & attack::knight_map[sq]);
	pawn_quiet(empty & attack::king_map[sq] & attack::slide_map[BISHOP][sq] & attack::in_front[pos.xturn][sq]);

	// generating discovered checks now, this is done by finding all pinned pieces of the same color
	// as the pinning piece and considering only moves of the pinned piece that break the pin
	// legal move generation is assumed

	uint64 pseudo_pin[64]{};
	attack::pins(pos, pos.xturn, pos.turn, pseudo_pin);

	for (int sq{ H1 }; sq <= A8; ++sq)
		pin[sq] = { pseudo_pin[sq] ? pin[sq] | ~pseudo_pin[sq] : std::numeric_limits<uint64>::max() };
	static_cast<void>(gen_quiet());

	return moves;
}

int gen::restore_loosing()
{
	// restoring loosing captures (copying them back from the bottom of the movelist)

	for (int i{}; i < count.loosing; ++i)
		move[i] = move[lim::moves - 1 - i];

	assert(moves == 0);
	assert(count.capture == 0);
	assert(count.promo == 0);

	moves = count.loosing;
	return moves;
}

int gen::restore_deferred(uint32 deferred[])
{
	// restoring deferred moves

	assert(moves == 0);
	while (deferred[moves] != MOVE_NONE)
	{
		move[moves] = deferred[moves];
		moves += 1;
	}
	return moves;
}

// internal move generating functions

template<promo_mode promo>
void gen::pawn_promo()
{
	// capturing west to promotion

	auto targets{ bit::shift(pos.pieces[PAWNS] & pos.side[pos.turn] & ~bit::file_west[pos.turn], shift::capture_west[pos.turn])
		& evasions & bit::rank_promo & pos.side[pos.xturn] };
	while (targets)
	{
		auto target{ bit::scan(targets) };
		auto origin{ target - shift::capture_west[pos.turn * 2] };
		assert((1ULL << origin) & pos.pieces[PAWNS] & pos.side[pos.turn]);

		if ((1ULL << target) & ~pin[origin])
		{
			for (int flag{ PROMO_QUEEN }; flag >= promo; --flag)
				move[moves++] = move::encode(origin, target, flag, PAWNS, pos.piece[target], pos.turn);
		}
		targets &= targets - 1;
	}

	// capturing east to promotion

	targets = bit::shift(pos.pieces[PAWNS] & pos.side[pos.turn] & ~bit::file_east[pos.turn], shift::capture_east[pos.turn])
		& evasions & bit::rank_promo & pos.side[pos.xturn];
	while (targets)
	{
		auto target{ bit::scan(targets) };
		auto origin{ target - shift::capture_east[pos.turn * 2] };
		assert((1ULL << origin) & pos.pieces[PAWNS] & pos.side[pos.turn]);

		if ((1ULL << target) & ~pin[origin])
		{
			for (int flag{ PROMO_QUEEN }; flag >= promo; --flag)
				move[moves++] = move::encode(origin, target, flag, PAWNS, pos.piece[target], pos.turn);
		}
		targets &= targets - 1;
	}

	// single push to promotion

	targets = bit::shift(pos.pieces[PAWNS] & pos.side[pos.turn], shift::push[pos.turn]) & ~pos.side[BOTH] & evasions & bit::rank_promo;
	while (targets)
	{
		auto target{ bit::scan(targets) };
		auto origin{ target - shift::push[pos.turn * 2] };
		assert((1ULL << origin) & pos.pieces[PAWNS] & pos.side[pos.turn]);

		if ((1ULL << target) & ~pin[origin])
		{
			for (int flag{ PROMO_QUEEN }; flag >= promo; --flag)
				move[moves++] = move::encode(origin, target, flag, PAWNS, NONE, pos.turn);
		}
		targets &= targets - 1;
	}
}

void gen::pawn_capture()
{
	// capturing west

	auto targets{ bit::shift(pos.pieces[PAWNS] & pos.side[pos.turn] & ~bit::file_west[pos.turn], shift::capture_west[pos.turn])
		& ~bit::rank_promo };
	auto targets_cap{ targets & pos.side[pos.xturn] & evasions };

	while (targets_cap)
	{
		auto target{ bit::scan(targets_cap) };
		auto origin{ target - shift::capture_west[pos.turn * 2] };
		assert((1ULL << origin) & pos.pieces[PAWNS] & pos.side[pos.turn]);

		if ((1ULL << target) & ~pin[origin])
			move[moves++] = move::encode(origin, target, NONE, PAWNS, pos.piece[target], pos.turn);

		targets_cap &= targets_cap - 1;
	}

	// capturing en-passant west

	auto target_ep{ targets & pos.ep_rear & bit::shift(evasions, shift::push[pos.turn]) };
	if (target_ep)
	{
		auto target{ bit::scan(target_ep) };
		auto origin{ target - shift::capture_west[pos.turn * 2] };
		assert((1ULL << origin) & pos.pieces[PAWNS] & pos.side[pos.turn]);

		target_ep &= ~pin[origin];
		if (target_ep)
			move[moves++] = move::encode(origin, target, ENPASSANT, PAWNS, PAWNS, pos.turn);
	}

	// capturing east

	targets = bit::shift(pos.pieces[PAWNS] & pos.side[pos.turn] & ~bit::file_east[pos.turn], shift::capture_east[pos.turn])
		& ~bit::rank_promo;
	targets_cap = targets & pos.side[pos.xturn] & evasions;

	while (targets_cap)
	{
		auto target{ bit::scan(targets_cap) };
		auto origin{ target - shift::capture_east[pos.turn * 2] };
		assert((1ULL << origin) & pos.pieces[PAWNS] & pos.side[pos.turn]);

		if ((1ULL << target) & ~pin[origin])
			move[moves++] = move::encode(origin, target, NONE, PAWNS, pos.piece[target], pos.turn);

		targets_cap &= targets_cap - 1;
	}

	// capturing en-passant east

	target_ep = targets & pos.ep_rear & bit::shift(evasions, shift::push[pos.turn]);
	if (target_ep)
	{
		auto target{ bit::scan(target_ep) };
		auto origin{ target - shift::capture_east[pos.turn * 2] };
		assert((1ULL << origin) & pos.pieces[PAWNS] & pos.side[pos.turn]);

		target_ep &= ~pin[origin];
		if (target_ep)
			move[moves++] = move::encode(origin, target, ENPASSANT, PAWNS, PAWNS, pos.turn);
	}
}

void gen::pawn_quiet(uint64 mask)
{
	// single push

	auto pushes{ bit::shift(pos.pieces[PAWNS] & pos.side[pos.turn], shift::push[pos.turn]) & mask & ~bit::rank_promo };
	auto targets{ pushes & evasions };

	while (targets)
	{
		auto target{ bit::scan(targets) };
		auto origin{ target - shift::push[pos.turn * 2] };
		assert((1ULL << origin) & pos.pieces[PAWNS] & pos.side[pos.turn]);

		if ((1ULL << target) & ~pin[origin])
			move[moves++] = move::encode(origin, target, NONE, PAWNS, NONE, pos.turn);

		targets &= targets - 1;
	}

	// double push

	auto targets2x{ bit::shift(pushes & bit::rank[relative::rank(R3, pos.turn)], shift::push[pos.turn]) & evasions & mask };
	while (targets2x)
	{
		auto target{ bit::scan(targets2x) };
		auto origin{ target - shift::push2x[pos.turn * 2] };
		assert((1ULL << origin) & pos.pieces[PAWNS] & pos.side[pos.turn]);

		if ((1ULL << target) & ~pin[origin])
			move[moves++] = move::encode(origin, target, DOUBLEPUSH, PAWNS, NONE, pos.turn);

		targets2x &= targets2x - 1;
	}
}

void gen::knight(uint64 mask)
{
	auto pieces{ pos.pieces[KNIGHTS] & pos.side[pos.turn] & king_color };
	while (pieces)
	{
		auto sq1{ bit::scan(pieces) };
		auto targets{ attack::knight_map[sq1] & mask & evasions & ~pin[sq1] };

		while (targets)
		{
			auto sq2{ bit::scan(targets) };
			move[moves++] = move::encode(sq1, sq2, NONE, KNIGHTS, pos.piece[sq2], pos.turn);
			targets &= targets - 1;
		}
		pieces &= pieces - 1;
	}
}

void gen::bishop(uint64 mask)
{
	auto pieces{ pos.pieces[BISHOPS] & pos.side[pos.turn] & king_color };
	while (pieces)
	{
		auto sq1{ bit::scan(pieces) };
		auto targets{ attack::by_slider<BISHOP>(sq1, pos.side[BOTH]) & mask & evasions & ~pin[sq1] };

		while (targets)
		{
			auto sq2{ bit::scan(targets) };
			move[moves++] = move::encode(sq1, sq2, NONE, BISHOPS, pos.piece[sq2], pos.turn);
			targets &= targets - 1;
		}
		pieces &= pieces - 1;
	}
}

void gen::rook(uint64 mask)
{
	auto pieces{ pos.pieces[ROOKS] & pos.side[pos.turn] };
	while (pieces)
	{
		auto sq1{ bit::scan(pieces) };
		auto targets{ attack::by_slider<ROOK>(sq1, pos.side[BOTH]) & mask & evasions & ~pin[sq1] };

		while (targets)
		{
			auto sq2{ bit::scan(targets) };
			move[moves++] = move::encode(sq1, sq2, NONE, ROOKS, pos.piece[sq2], pos.turn);
			targets &= targets - 1;
		}
		pieces &= pieces - 1;
	}
}

void gen::queen(uint64 mask)
{
	auto pieces{ pos.pieces[QUEENS] & pos.side[pos.turn] };
	while (pieces)
	{
		auto sq1{ bit::scan(pieces) };
		auto targets{ attack::by_slider<QUEEN>(sq1, pos.side[BOTH]) & mask & evasions & ~pin[sq1] };

		while (targets)
		{
			auto sq2{ bit::scan(targets) };
			move[moves++] = move::encode(sq1, sq2, NONE, QUEENS, pos.piece[sq2], pos.turn);
			targets &= targets - 1;
		}
		pieces &= pieces - 1;
	}
}

void gen::king(uint64 mask)
{
	// normal king moves

	auto sq1{ pos.sq_king[pos.turn] };
	auto targets{ attack::check(pos, pos.turn, attack::king_map[sq1] & mask & ~pin[sq1]) };
	while (targets)
	{
		auto sq2{ bit::scan(targets) };
		move[moves++] = move::encode(sq1, sq2, NONE, KINGS, pos.piece[sq2], pos.turn);
		targets &= targets - 1;
	}

	// castling moves, chess960 compliant and an ugly mess

	if (mask != pos.side[pos.xturn] && ~pin[sq1])
	{
		// castling king-side

		if (pos.castling_right[pos.turn] != PROHIBITED)
		{
			auto king_target_bit{ 1ULL << square::king_target[pos.turn][0] };
			auto rook_bit{ 1ULL << pos.castling_right[pos.turn] };

			auto bound_max{ 1ULL << std::max(static_cast<uint8>(square::rook_target[pos.turn][0] + 1), sq1) };
			auto bound_min{ std::min(king_target_bit, rook_bit) };
			auto occupancy{ pos.side[BOTH] ^ (1ULL << sq1 | rook_bit) };

			if (!(((bound_max - 1) & ~(bound_min - 1)) & occupancy))
			{
				auto no_check_zone{ ((1ULL << (sq1 + 1)) - 1) & ~(king_target_bit - 1) };
				pos.side[BOTH] ^= rook_bit;

				if(attack::check(pos, pos.turn, no_check_zone) == no_check_zone)
					move[moves++] = move::encode(sq1, pos.castling_right[pos.turn], CASTLE_SHORT, KINGS, NONE, pos.turn);

				pos.side[BOTH] ^= rook_bit;
			}
		}

		// castling queen-side, even a bigger mess

		if (pos.castling_right[pos.turn + 2] != PROHIBITED)
		{
			auto rook_bit{ 1ULL << pos.castling_right[pos.turn + 2] };

			auto bound_max{ 1ULL << std::max(static_cast<uint8>(square::king_target[pos.turn][1] + 1), pos.castling_right[pos.turn + 2]) };
			auto bound_min{ 1ULL << std::min(square::rook_target[pos.turn][1], sq1) };
			auto occupancy{ pos.side[BOTH] ^ ((1ULL << sq1) | rook_bit) };

			if (!(((bound_max - 1) & ~(bound_min - 1)) & occupancy))
			{
				auto no_check_zone
				{
					(1ULL << sq1) | (1ULL << square::king_target[pos.turn][1])
					| (((1ULL << square::king_target[pos.turn][1]) - 1) & ~((1ULL << sq1) - 1))
				};
				pos.side[BOTH] ^= rook_bit;

				if (attack::check(pos, pos.turn, no_check_zone) == no_check_zone)
					move[moves++] = move::encode(sq1, pos.castling_right[pos.turn + 2], CASTLE_LONG, KINGS, NONE, pos.turn);

				pos.side[BOTH] ^= rook_bit;
			}
		}
	}
}
