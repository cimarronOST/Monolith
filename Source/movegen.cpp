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


#include "move.h"
#include "attack.h"
#include "bit.h"
#include "magic.h"
#include "movegen.h"

namespace
{
	// speeding up things through fast array indexing

	const uint8 king_target[]{ G1, G8, C1, C8 };
	const uint8 rook_target[]{ F1, F8, D1, D8 };

	// speeding up pawn move generation

	const int cap_left[]   {  9, 55,  -9 };
	const int cap_right[]  {  7, 57,  -7 };
	const int push[]       {  8, 56,  -8 };
	const int double_push[]{ 16, 48, -16 };

	const uint64 promo_rank{ bit::rank[R1] | bit::rank[R8] };

	const uint64 boarder_left[] { bit::file[A], bit::file[H] };
	const uint64 boarder_right[]{ bit::file[H], bit::file[A] };
}

namespace color
{
	uint64 king(const board &pos, int turn)
	{
		return (pos.pieces[KINGS] & pos.side[turn] & square::white) ? square::white : square::black;
	}
}

void gen::gen_all()
{
	// root node & perft & UCI move generation
	// all moves indepented of their stages are generated

	gen_tactical<PROMO_ALL>();
	gen_quiet();
}

void gen::gen_searchmoves(std::vector<uint32> &searchmoves)
{
	// reacting to the UCI 'go searchmoves' command

	std::copy(searchmoves.begin(), searchmoves.end(), move);
	cnt.moves = static_cast<int>(searchmoves.size());
}

// external move generating functions called by movepick

int gen::gen_hash(uint32 &hash_move)
{
	// "generating" the hash move

	assert(cnt.moves == 0);

	if (hash_move != NO_MOVE)
	{
		if (pos.pseudolegal(hash_move))
			move[cnt.moves++] = hash_move;
		else
			hash_move = NO_MOVE;
	}
	return cnt.moves;
}

template int gen::gen_tactical<PROMO_ALL>();
template int gen::gen_tactical<PROMO_QUEEN>();

template<promo_mode promo>
int gen::gen_tactical()
{
	// generating captures

	assert(cnt.moves == 0);
	assert(cnt.capture == 0);
	auto targets{ pos.side[pos.xturn] };

	king(targets);
	pawn_capt();
	queen(targets);
	rook(targets);
	bishop(targets);
	knight(targets);
	cnt.capture = cnt.moves;

	// generating promotions

	pawn_promo<promo>();
	cnt.promo = cnt.moves - cnt.capture;

	return cnt.moves;
}

int gen::gen_killer(uint32 killer[][2], int depth, uint32 counter)
{
	// "generating" quiet killer moves

	assert(cnt.moves == 0);
	for (auto slot{ 0 }; slot < 2; ++slot)
	{
		if (pos.pseudolegal(killer[depth][slot]))
			move[cnt.moves++] = killer[depth][slot];
	}

	// "generating" the counter move

	if (pos.pseudolegal(counter) && counter != killer[depth][0] && counter != killer[depth][1])
		move[cnt.moves++] = counter;

	return cnt.moves;
}

int gen::gen_quiet()
{
	// generating quiet moves

	assert(cnt.capture + cnt.promo == cnt.moves);
	auto targets{ ~pos.side[BOTH] };

	king(targets);
	queen(targets);
	rook(targets);
	bishop(targets);
	knight(targets);
	pawn_quiet(targets);

	return cnt.moves;
}

int gen::gen_loosing()
{
	// "generating" loosing captures (copying them back from the bottom of the movelist)

	for (auto i{ 0 }; i < cnt.loosing; ++i)
		move[i] = move[lim::moves - 1 - i];

	assert(cnt.moves == 0);
	assert(cnt.capture == 0);
	assert(cnt.promo == 0);

	cnt.moves = cnt.loosing;
	return cnt.moves;
}

int gen::gen_check()
{
	// generating quiet checking moves

	assert(cnt.moves == 0);
	assert(cnt.capture == 0);

	// direct checks

	auto sq{ pos.king_sq[pos.xturn] };
	auto empty{ ~pos.side[BOTH] };
	auto reach{ empty & attack::by_slider<QUEEN>(sq, pos.side[BOTH]) };
	king_color = color::king(pos, pos.xturn);

	pawn_quiet(empty & attack::king_map[sq] & attack::slide_map[BISHOP][sq] & attack::in_front[pos.xturn][sq]);
	knight(empty & attack::knight_map[sq]);
	bishop(reach & attack::slide_map[BISHOP][sq]);
	rook(reach & attack::slide_map[ROOK][sq]);
	queen(reach);

	return cnt.moves;
}

// internal move generating functions

template<promo_mode promo>
void gen::pawn_promo()
{
	// capturing left & promoting

	auto targets{ bit::shift(pos.pieces[PAWNS] & pos.side[pos.turn] & ~boarder_left[pos.turn], cap_left[pos.turn])
		& evasions & promo_rank & pos.side[pos.xturn] };
	while (targets)
	{
		auto target{ bit::scan(targets) };
		auto origin{ target - cap_left[pos.turn << 1] };
		assert((1ULL << origin) & pos.pieces[PAWNS] & pos.side[pos.turn]);

		if ((1ULL << target) & ~pin[origin])
		{
			for (int flag{ PROMO_QUEEN }; flag >= promo; --flag)
				move[cnt.moves++] = move::encode(origin, target, flag, PAWNS, pos.piece_sq[target], pos.turn);
		}
		targets &= targets - 1;
	}

	// capturing right & promoting

	targets = bit::shift(pos.pieces[PAWNS] & pos.side[pos.turn] & ~boarder_right[pos.turn], cap_right[pos.turn])
		& evasions & promo_rank & pos.side[pos.xturn];
	while (targets)
	{
		auto target{ bit::scan(targets) };
		auto origin{ target - cap_right[pos.turn << 1] };
		assert((1ULL << origin) & pos.pieces[PAWNS] & pos.side[pos.turn]);

		if ((1ULL << target) & ~pin[origin])
		{
			for (int flag{ PROMO_QUEEN }; flag >= promo; --flag)
				move[cnt.moves++] = move::encode(origin, target, flag, PAWNS, pos.piece_sq[target], pos.turn);
		}
		targets &= targets - 1;
	}

	// single push to promotion

	targets = bit::shift(pos.pieces[PAWNS] & pos.side[pos.turn], push[pos.turn]) & ~pos.side[BOTH] & evasions & promo_rank;
	while (targets)
	{
		auto target{ bit::scan(targets) };
		auto origin{ target - push[pos.turn << 1] };
		assert((1ULL << origin) & pos.pieces[PAWNS] & pos.side[pos.turn]);

		if ((1ULL << target) & ~pin[origin])
		{
			for (int flag{ PROMO_QUEEN }; flag >= promo; --flag)
				move[cnt.moves++] = move::encode(origin, target, flag, PAWNS, NONE, pos.turn);
		}
		targets &= targets - 1;
	}
}

void gen::pawn_capt()
{
	// capturing left

	auto targets{ bit::shift(pos.pieces[PAWNS] & pos.side[pos.turn] & ~boarder_left[pos.turn], cap_left[pos.turn]) & ~promo_rank };
	auto targets_cap{ targets & pos.side[pos.xturn] & evasions };

	while (targets_cap)
	{
		auto target{ bit::scan(targets_cap) };
		auto origin{ target - cap_left[pos.turn << 1] };
		assert((1ULL << origin) & pos.pieces[PAWNS] & pos.side[pos.turn]);

		if ((1ULL << target) & ~pin[origin])
			move[cnt.moves++] = move::encode(origin, target, NONE, PAWNS, pos.piece_sq[target], pos.turn);

		targets_cap &= targets_cap - 1;
	}

	// enpassant left

	auto target_ep{ targets & pos.ep_sq & bit::shift(evasions, push[pos.turn]) };
	if (target_ep)
	{
		auto target{ bit::scan(target_ep) };
		auto origin{ target - cap_left[pos.turn << 1] };
		assert((1ULL << origin) & pos.pieces[PAWNS] & pos.side[pos.turn]);

		target_ep &= ~pin[origin];
		if (target_ep)
			move[cnt.moves++] = move::encode(origin, target, ENPASSANT, PAWNS, PAWNS, pos.turn);
	}

	// capturing right

	targets = bit::shift(pos.pieces[PAWNS] & pos.side[pos.turn] & ~boarder_right[pos.turn], cap_right[pos.turn]) & ~promo_rank;
	targets_cap = targets & pos.side[pos.xturn] & evasions;

	while (targets_cap)
	{
		auto target{ bit::scan(targets_cap) };
		auto origin{ target - cap_right[pos.turn << 1] };
		assert((1ULL << origin) & pos.pieces[PAWNS] & pos.side[pos.turn]);

		if ((1ULL << target) & ~pin[origin])
			move[cnt.moves++] = move::encode(origin, target, NONE, PAWNS, pos.piece_sq[target], pos.turn);

		targets_cap &= targets_cap - 1;
	}

	// enpassant right

	target_ep = targets & pos.ep_sq & bit::shift(evasions, push[pos.turn]);
	if (target_ep)
	{
		auto target{ bit::scan(target_ep) };
		auto origin{ target - cap_right[pos.turn << 1] };
		assert((1ULL << origin) & pos.pieces[PAWNS] & pos.side[pos.turn]);

		target_ep &= ~pin[origin];
		if (target_ep)
			move[cnt.moves++] = move::encode(origin, target, ENPASSANT, PAWNS, PAWNS, pos.turn);
	}
}

void gen::pawn_quiet(uint64 mask)
{
	// single push

	auto pushes{ bit::shift(pos.pieces[PAWNS] & pos.side[pos.turn], push[pos.turn]) & mask & ~promo_rank };
	auto targets{ pushes & evasions };

	while (targets)
	{
		auto target{ bit::scan(targets) };
		auto origin{ target - push[pos.turn << 1] };
		assert((1ULL << origin) & pos.pieces[PAWNS] & pos.side[pos.turn]);

		if ((1ULL << target) & ~pin[origin])
			move[cnt.moves++] = move::encode(origin, target, NONE, PAWNS, NONE, pos.turn);

		targets &= targets - 1;
	}

	// double push

	auto targets2x{ bit::shift(pushes & bit::rank[R3 + 3 * pos.turn], push[pos.turn]) & evasions & mask };
	while (targets2x)
	{
		auto target{ bit::scan(targets2x) };
		auto origin{ target - double_push[pos.turn << 1] };
		assert((1ULL << origin) & pos.pieces[PAWNS] & pos.side[pos.turn]);

		if ((1ULL << target) & ~pin[origin])
			move[cnt.moves++] = move::encode(origin, target, DOUBLEPUSH, PAWNS, NONE, pos.turn);

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
			move[cnt.moves++] = move::encode(sq1, sq2, NONE, KNIGHTS, pos.piece_sq[sq2], pos.turn);
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
			move[cnt.moves++] = move::encode(sq1, sq2, NONE, BISHOPS, pos.piece_sq[sq2], pos.turn);
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
			move[cnt.moves++] = move::encode(sq1, sq2, NONE, ROOKS, pos.piece_sq[sq2], pos.turn);
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
			move[cnt.moves++] = move::encode(sq1, sq2, NONE, QUEENS, pos.piece_sq[sq2], pos.turn);
			targets &= targets - 1;
		}
		pieces &= pieces - 1;
	}
}

void gen::king(uint64 mask)
{
	// normal king moves

	auto targets{ attack::check(pos, pos.turn, attack::king_map[pos.king_sq[pos.turn]] & mask) };
	while (targets)
	{
		auto sq{ bit::scan(targets) };
		move[cnt.moves++] = move::encode(pos.king_sq[pos.turn], sq, NONE, KINGS, pos.piece_sq[sq], pos.turn);
		targets &= targets - 1;
	}

	// castling moves, chess960 compliant and therefore messy

	if (mask != pos.side[pos.xturn])
	{
		// castling kingside

		if (pos.castling_right[pos.turn] != PROHIBITED)
		{
			auto king_target_64{ 1ULL << king_target[pos.turn] };
			auto rook_64{ 1ULL << pos.castling_right[pos.turn] };

			auto bound_max{ 1ULL << std::max(static_cast<uint8>(rook_target[pos.turn] + 1), pos.king_sq[pos.turn]) };
			auto bound_min{ std::min(king_target_64, rook_64) };
			auto occupancy{ pos.side[BOTH] ^ (1ULL << pos.king_sq[pos.turn] | rook_64) };

			if (!(((bound_max - 1) & ~(bound_min - 1)) & occupancy))
			{
				auto no_check_zone{ ((1ULL << (pos.king_sq[pos.turn] + 1)) - 1) & ~(king_target_64 - 1) };
				pos.side[BOTH] ^= rook_64;

				if(attack::check(pos, pos.turn, no_check_zone) == no_check_zone)
					move[cnt.moves++] = move::encode(pos.king_sq[pos.turn], pos.castling_right[pos.turn], castling::SHORT, KINGS, NONE, pos.turn);

				pos.side[BOTH] ^= rook_64;
			}
		}

		// castling queenside

		if (pos.castling_right[pos.turn + 2] != PROHIBITED)
		{
			auto rook_64{ 1ULL << pos.castling_right[pos.turn + 2] };

			auto bound_max{ 1ULL << std::max(static_cast<uint8>(king_target[pos.turn + 2] + 1), pos.castling_right[pos.turn + 2]) };
			auto bound_min{ 1ULL << std::min(rook_target[pos.turn + 2], pos.king_sq[pos.turn]) };
			auto occupancy{ pos.side[BOTH] ^ ((1ULL << pos.king_sq[pos.turn]) | rook_64) };

			if (!(((bound_max - 1) & ~(bound_min - 1)) & occupancy))
			{
				auto no_check_zone
				{
					(1ULL << pos.king_sq[pos.turn]) | (1ULL << king_target[pos.turn + 2])
					| (((1ULL << king_target[pos.turn + 2]) - 1) & ~((1ULL << pos.king_sq[pos.turn]) - 1))
				};
				pos.side[BOTH] ^= rook_64;

				if (attack::check(pos, pos.turn, no_check_zone) == no_check_zone)
					move[cnt.moves++] = move::encode(pos.king_sq[pos.turn], pos.castling_right[pos.turn + 2], castling::LONG, KINGS, NONE, pos.turn);

				pos.side[BOTH] ^= rook_64;
			}
		}
	}
}

// preparing legal move generation

void gen::find_evasions()
{
	// defining the evasion zone if the king is under attack

	auto &sq{ pos.king_sq[pos.turn] };
	auto att
	{
		pos.side[pos.xturn]
		& ((pos.pieces[PAWNS] & attack::king_map[sq] & attack::slide_map[BISHOP][sq] & attack::in_front[pos.turn][sq])
		|  (pos.pieces[KNIGHTS] & attack::knight_map[sq])
		| ((pos.pieces[BISHOPS] | pos.pieces[QUEENS]) & attack::by_slider<BISHOP>(sq, pos.side[BOTH]))
		| ((pos.pieces[ROOKS] | pos.pieces[QUEENS]) & attack::by_slider<ROOK>(sq, pos.side[BOTH])))
	};
	auto nr_att{ bit::popcnt(att) };

	// no attacker, no evasion moves necessary

	if (nr_att == 0)
		evasions = ~0ULL;

	// 1 attacker, only evasions that block the path or capture the attacker are legal

	else if (nr_att == 1)
	{
		if (att & pos.pieces[KNIGHTS] || att & pos.pieces[PAWNS])
			evasions = att;
		else
		{
			assert(att & pos.pieces[ROOKS] || att & pos.pieces[BISHOPS] || att & pos.pieces[QUEENS]);
			auto every_att{ attack::by_slider<QUEEN>(sq, pos.side[BOTH]) };

			auto sq64{ 1ULL << sq };
			assert(sq64 == (pos.pieces[KINGS] & pos.side[pos.turn]));

			for (auto dir{ 0 }; dir < 8; ++dir)
			{
				auto flood{ sq64 };
				for (; !(flood & magic::ray[dir].boarder); flood |= bit::shift(flood, magic::ray[dir].shift)) ;

				if (flood & att)
				{
					evasions = flood & every_att;
					break;
				}
			}
		}
	}

	// 2 attackers, only king move evasions are legal

	else
	{
		assert(nr_att == 2);
		evasions = 0ULL;
	}
}

void gen::find_pins()
{
	// finding all pieces that are pinned to the king and defining their legal move zone

	auto sq64{ pos.pieces[KINGS] & pos.side[pos.turn] };
	auto att
	{
		pos.side[pos.xturn]
		& (((pos.pieces[BISHOPS] | pos.pieces[QUEENS]) & attack::slide_map[BISHOP][pos.king_sq[pos.turn]])
		|  ((pos.pieces[ROOKS] | pos.pieces[QUEENS]) & attack::slide_map[ROOK][pos.king_sq[pos.turn]]))
	};

	while (att)
	{
		// generating rays centered on the king square

		auto ray_to_att{ attack::by_slider<QUEEN>(pos.king_sq[pos.turn], att) };
		auto attacker{ 1ULL << bit::scan(att) };

		if (!(attacker & ray_to_att))
		{
			att &= att - 1;
			continue;
		}

		// creating final ray from king to attacker

		assert(sq64 != 0ULL);

		auto x_ray{ 0ULL };
		for (auto dir{ 0 }; dir < 8; ++dir)
		{
			auto flood{ sq64 };
			for (; !(flood & magic::ray[dir].boarder); flood |= bit::shift(flood, magic::ray[dir].shift));
			if (flood & attacker)
			{
				x_ray = flood & ray_to_att;
				break;
			}
		}

		assert(x_ray & attacker);
		assert(!(x_ray & sq64));

		// allowing only moves inside the pinning ray

		if ((x_ray & pos.side[pos.turn]) && bit::popcnt(x_ray & pos.side[BOTH]) == 2)
		{
			assert(bit::popcnt(x_ray & pos.side[pos.turn]) == 1);

			auto sq{ bit::scan(x_ray & pos.side[pos.turn]) };
			pin[sq] = ~x_ray;
		}

		// prohibiting enpassant in seldom pin cases

		else if (pos.ep_sq
			&& x_ray & pos.side[pos.turn] & pos.pieces[PAWNS]
			&& x_ray & pos.side[pos.xturn] & pos.pieces[PAWNS]
			&& bit::popcnt(x_ray & pos.side[BOTH]) == 3)
		{
			assert(bit::popcnt(x_ray & pos.side[pos.xturn]) == 2);

			auto enemy_pawn{ x_ray & pos.side[pos.xturn] & pos.pieces[PAWNS] };
			auto friend_pawn{ x_ray & pos.side[pos.turn] & pos.pieces[PAWNS] };

			if (friend_pawn << 1 == enemy_pawn || friend_pawn >> 1 == enemy_pawn)
			{
				if (pos.ep_sq == bit::shift(enemy_pawn, push[pos.turn]))
				{
					auto sq{ bit::scan(x_ray & pos.side[pos.turn]) };
					pin[sq] = pos.ep_sq;
				}
			}
		}
		att &= att - 1;
	}
}

// finding moves in the movelist

uint32 *gen::find(uint32 move)
{
	return std::find(this->move, this->move + cnt.moves, move);
}

bool gen::in_list(uint32 move)
{
	return find(move) != this->move + cnt.moves;
}