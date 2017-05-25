/*
  Monolith 0.2  Copyright (C) 2017 Jonas Mayr

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

#include "random.h"
#include "bitboard.h"
#include "movegen.h"

uint64 movegen::knight_table[64]{ 0ULL };
uint64 movegen::king_table[64]{ 0ULL };
uint64 movegen::pinned[64];
uint64 movegen::legal_sq;

namespace
{
	inline void real_shift(uint64 &bb, int shift)
	{
		bb = (bb << shift) | (bb >> (64 - shift));
	}

	// magic variables

	magic::entry slider[2][64];
	uint64 slide_ray[2][64];

	vector<uint64> attack_table;
	const int table_size{ 107648 };

	const magic::pattern ray[]
	{
		{  8, 0xff00000000000000 }, {  7, 0xff01010101010101 },
		{ 63, 0x0101010101010101 }, { 55, 0x01010101010101ff },
		{ 56, 0x00000000000000ff }, { 57, 0x80808080808080ff },
		{  1, 0x8080808080808080 }, {  9, 0xff80808080808080 }
	};

	// movegen variables

	uint64 enemies, friends, fr_king, pawn_rank;
	uint64 not_right, not_left;
	uint64 gentype[2];

	uint8 AHEAD, BACK;

	uint32 pin_idx[8];
	int pin_cnt;

	const int cap_left[]{ 9, 55, -9 };
	const int cap_right[]{ 7, 57, -7 };
	const int push[]{ 8, 56, -8 };
	const int double_push[]{ 16, 48, -16 };

	const uint64 promo_rank{ rank[R1] | rank[R8] };
	const uint64 third_rank[]{ rank[R3], rank[R6] };
	const uint64 boarder[]{ file[A], file[H] };
}

// movegen functions

void movegen::init()
{
	std::fill(pinned, pinned + 64, 0xffffffffffffffff);
}

int movegen::gen_moves(pos &board, gen_e type)
{
	// pawn bitboards

	AHEAD = board.turn;
	BACK = board.turn * 2;

	not_right = ~boarder[board.turn ^ 1];
	not_left  = ~boarder[board.turn];
	pawn_rank = third_rank[board.turn];

	friends = board.side[board.turn];
	enemies = board.side[board.turn ^ 1];
	fr_king = board.pieces[KINGS] & friends;

	legal init(board);

	gentype[CAPTURES] = enemies;
	gentype[QUIETS] = ~board.side[BOTH];
	move_cnt = promo_cnt = capt_cnt = 0;

	// generating captures

	king_moves(board, CAPTURES);
	pawn_capt(board);
	piece_moves(board, CAPTURES);
	capt_cnt = move_cnt;

	// generating promotions

	pawn_promo(board);
	promo_cnt = move_cnt - capt_cnt;

	// generating quiets

	if (type == ALL)
	{
		pawn_quiet(board);
		piece_moves(board, QUIETS);
		king_moves(board, QUIETS);
	}

	return move_cnt;
}

void movegen::pawn_promo(pos &board)
{
	// capture left

	uint64 targets{ shift(board.pieces[PAWNS] & friends & not_left, cap_left[AHEAD]) & legal_sq & promo_rank & enemies };
	while (targets)
	{
		bb::bitscan(targets);
		uint64 target{ 1ULL << bb::lsb() };
		auto target_sq{ bb::lsb() };
		auto origin_sq{ target_sq - cap_left[BACK] };

		assert((1ULL << origin_sq) & board.pieces[PAWNS] & friends);

		target &= pinned[origin_sq];
		if (target)
		{
			for (int flag{ 15 }; flag >= 12; --flag)
				list[move_cnt++] = encode(origin_sq, target_sq, flag);
		}

		targets &= targets - 1;
	}

	// capture right

	targets = shift(board.pieces[PAWNS] & friends & not_right, cap_right[AHEAD]) & legal_sq & promo_rank & enemies;
	while (targets)
	{
		bb::bitscan(targets);
		uint64 target{ 1ULL << bb::lsb() };
		auto target_sq{ bb::lsb() };
		auto origin_sq{ target_sq - cap_right[BACK] };

		assert((1ULL << origin_sq) & board.pieces[PAWNS] & friends);

		target &= pinned[origin_sq];
		if (target)
		{
			for (int flag{ 15 }; flag >= 12; --flag)
				list[move_cnt++] = encode(origin_sq, target_sq, flag);
		}

		targets &= targets - 1;
	}

	// single push

	targets = shift(board.pieces[PAWNS] & friends, push[AHEAD]) & ~board.side[BOTH] & legal_sq & promo_rank;
	while (targets)
	{
		bb::bitscan(targets);
		uint64 target{ 1ULL << bb::lsb() };
		auto target_sq{ bb::lsb() };
		auto origin_sq{ target_sq - push[BACK] };

		assert((1ULL << origin_sq) & board.pieces[PAWNS] & friends);

		target &= pinned[origin_sq];
		if (target)
		{
			for (int flag{ 15 }; flag >= 12; --flag)
				list[move_cnt++] = encode(origin_sq, target_sq, flag);
		}

		targets &= targets - 1;
	}
}
void movegen::pawn_capt(pos &board)
{
	uint64 targets{ shift(board.pieces[PAWNS] & friends & not_left, cap_left[AHEAD]) & ~promo_rank };
	uint64 targets_cap{ targets & enemies & legal_sq };

	while (targets_cap)
	{
		bb::bitscan(targets_cap);
		uint64 target{ 1ULL << bb::lsb() };
		auto target_sq{ bb::lsb() };
		auto origin_sq{ target_sq - cap_left[BACK] };

		assert((1ULL << origin_sq) & board.pieces[PAWNS] & friends);

		target &= pinned[origin_sq];
		if (target)
			list[move_cnt++] = encode(origin_sq, target_sq, board.piece_sq[target_sq]);

		targets_cap &= targets_cap - 1;
	}

	uint64 target_ep{ targets & board.ep_square & shift(legal_sq, push[AHEAD]) };
	if (target_ep)
	{
		bb::bitscan(target_ep);
		auto target_sq{ bb::lsb() };
		auto origin_sq{ target_sq - cap_left[BACK] };

		assert((1ULL << origin_sq) & board.pieces[PAWNS] & friends);

		target_ep &= pinned[origin_sq];
		if (target_ep)
			list[move_cnt++] = encode(origin_sq, target_sq, ENPASSANT);
	}

	targets = shift(board.pieces[PAWNS] & friends & not_right, cap_right[AHEAD]) & ~promo_rank;
	targets_cap = targets & enemies & legal_sq;

	while (targets_cap)
	{
		bb::bitscan(targets_cap);
		uint64 target{ 1ULL << bb::lsb() };
		auto target_sq{ bb::lsb() };
		auto origin_sq{ target_sq - cap_right[BACK] };

		assert((1ULL << origin_sq) & board.pieces[PAWNS] & friends);

		target &= pinned[origin_sq];
		if (target)
			list[move_cnt++] = encode(origin_sq, target_sq, board.piece_sq[target_sq]);

		targets_cap &= targets_cap - 1;
	}

	target_ep = targets & board.ep_square & shift(legal_sq, push[AHEAD]);
	if (target_ep)
	{
		bb::bitscan(target_ep);
		auto target_sq{ bb::lsb() };
		auto origin_sq{ target_sq - cap_right[BACK] };

		assert((1ULL << origin_sq) & board.pieces[PAWNS] & friends);

		target_ep &= pinned[origin_sq];
		if (target_ep)
			list[move_cnt++] = encode(origin_sq, target_sq, ENPASSANT);
	}
}
void movegen::pawn_quiet(pos &board)
{
	uint64 pushes{ shift(board.pieces[PAWNS] & friends, push[AHEAD]) & ~board.side[BOTH] & ~promo_rank };
	uint64 targets{ pushes & legal_sq };

	while (targets)
	{
		bb::bitscan(targets);
		uint64 target{ 1ULL << bb::lsb() };
		auto target_sq{ bb::lsb() };
		auto origin_sq{ target_sq - push[BACK] };

		assert((1ULL << origin_sq) & board.pieces[PAWNS] & friends);

		target &= pinned[origin_sq];
		if (target)
			list[move_cnt++] = encode(origin_sq, target_sq, NONE);

		targets &= targets - 1;
	}

	uint64 targets2x{ shift(pushes & pawn_rank, push[AHEAD]) & legal_sq & ~board.side[BOTH] };
	while (targets2x)
	{
		bb::bitscan(targets2x);
		uint64 target{ 1ULL << bb::lsb() };
		auto target_sq{ bb::lsb() };
		auto origin_sq{ target_sq - double_push[BACK] };

		assert((1ULL << origin_sq) & board.pieces[PAWNS] & friends);

		target &= pinned[origin_sq];
		if (target)
			list[move_cnt++] = encode(origin_sq, target_sq, NONE);

		targets2x &= targets2x - 1;
	}
}
void movegen::piece_moves(pos &board, gen_e type)
{
	uint64 targets{ 0 };

	// queens

	uint64 pieces{ board.pieces[QUEENS] & friends };
	while (pieces)
	{
		bb::bitscan(pieces);
		auto sq{ bb::lsb() };
		targets = slide_att(BISHOP, sq, board.side[BOTH]) | slide_att(ROOK, sq, board.side[BOTH]);
		targets &= gentype[type] & legal_sq & pinned[sq];

		while (targets)
		{
			bb::bitscan(targets);
			list[move_cnt++] = encode(sq, bb::lsb(), board.piece_sq[bb::lsb()]);
			targets &= targets - 1;
		}
		pieces &= pieces - 1;
	}

	// knights

	pieces = board.pieces[KNIGHTS] & friends;
	while (pieces)
	{
		bb::bitscan(pieces);
		auto sq{ bb::lsb() };

		targets = knight_table[sq] & gentype[type] & legal_sq & pinned[sq];
		while (targets)
		{
			bb::bitscan(targets);
			list[move_cnt++] = encode(sq, bb::lsb(), board.piece_sq[bb::lsb()]);
			targets &= targets - 1;
		}
		pieces &= pieces - 1;
	}

	// bishops

	pieces = board.pieces[BISHOPS] & friends;
	while (pieces)
	{
		bb::bitscan(pieces);
		auto sq{ bb::lsb() };

		targets = slide_att(BISHOP, sq, board.side[BOTH]) & gentype[type] & legal_sq & pinned[sq];
		while (targets)
		{
			bb::bitscan(targets);
			list[move_cnt++] = encode(sq, bb::lsb(), board.piece_sq[bb::lsb()]);
			targets &= targets - 1;
		}
		pieces &= pieces - 1;
	}

	// rooks

	pieces = board.pieces[ROOKS] & friends;
	while (pieces)
	{
		bb::bitscan(pieces);
		auto sq{ bb::lsb() };

		targets = slide_att(ROOK, sq, board.side[BOTH]) & gentype[type] & legal_sq & pinned[sq];
		while (targets)
		{
			bb::bitscan(targets);
			list[move_cnt++] = encode(sq, bb::lsb(), board.piece_sq[bb::lsb()]);
			targets &= targets - 1;
		}
		pieces &= pieces - 1;
	}
}
void movegen::king_moves(pos &board, gen_e type)
{
	uint64 targets{ check(board, board.turn, king_table[board.king_sq[board.turn]] & gentype[type]) };
	while (targets)
	{
		bb::bitscan(targets);
		list[move_cnt++] = encode(board.king_sq[board.turn], bb::lsb(), board.piece_sq[bb::lsb()]);
		targets &= targets - 1;
	}

	// castling

	if (type == QUIETS && fr_king & 0x800000000000008)
	{
		const uint64 rank_king{ rank[board.turn * 7] };

		const uint8 rights_s[]{ 0x1, 0x10 };
		const uint8 rights_l[]{ 0x4, 0x40 };

		if ((rights_s[board.turn] & board.castl_rights)
			&& !(board.side[BOTH] & 0x600000000000006 & rank_king)
			&& bb::popcnt(check(board, board.turn, 0x0e0000000000000e & rank_king)) == 3)
			{
				const uint32 target[]{ 1, 57 };
				list[move_cnt++] = encode(board.king_sq[board.turn], target[board.turn], (castl_e::SHORT_WHITE + board.turn * 2));
			}
		if ((rights_l[board.turn] & board.castl_rights)
			&& !(board.side[BOTH] & 0x7000000000000070 & rank_king)
			&& bb::popcnt(check(board, board.turn, 0x3800000000000038 & rank_king)) == 3)
			{
				const uint32 target[]{ 5, 61 };
				list[move_cnt++] = encode(board.king_sq[board.turn], target[board.turn], (castl_e::LONG_WHITE + board.turn * 2));
			}
	}
}

void movegen::init_legal(pos &board)
{
	const uint64 in_front[]{ ~(fr_king - 1), fr_king - 1 };
	auto king_sq{ board.king_sq[board.turn] };
	assert(fr_king != 0ULL);

	uint64 att{ slide_att(ROOK, king_sq, board.side[BOTH]) & (board.pieces[ROOKS] | board.pieces[QUEENS]) };
	att |= slide_att(BISHOP, king_sq, board.side[BOTH]) & (board.pieces[BISHOPS] | board.pieces[QUEENS]);
	att |= knight_table[king_sq] & board.pieces[KNIGHTS];
	att |= king_table[king_sq] & board.pieces[PAWNS] & slide_ray[BISHOP][king_sq] & in_front[board.turn];
	att &= enemies;

	int nr_att{ bb::popcnt(att) };

	if (nr_att == 0)
	{
		legal_sq = 0xffffffffffffffff;
	}
	else if (nr_att == 1)
	{
		if (att & board.pieces[KNIGHTS] || att & board.pieces[PAWNS])
		{
			legal_sq = att;
		}
		else
		{
			assert(att & board.pieces[ROOKS] || att & board.pieces[BISHOPS] || att & board.pieces[QUEENS]);
			auto every_att{ slide_att(ROOK, king_sq, board.side[BOTH]) | slide_att(BISHOP, king_sq, board.side[BOTH]) };

			for (int dir{ 0 }; dir < 8; ++dir)
			{
				auto flood{ fr_king };
				for (; !(flood & ray[dir].boarder); flood |= shift(flood, ray[dir].shift)) ;

				if (flood & att)
				{
					legal_sq = flood & every_att;
					break;
				}
			}
		}
	}
	else
	{
		assert(nr_att == 2);
		legal_sq = 0ULL;
	}
}
void movegen::init_pin(pos &board)
{
	pin_cnt = 0;
	auto king_sq{ board.king_sq[board.turn] };

	uint64 att{ slide_ray[ROOK][king_sq] & enemies & (board.pieces[ROOKS] | board.pieces[QUEENS]) };
	att |= slide_ray[BISHOP][king_sq] & enemies & (board.pieces[BISHOPS] | board.pieces[QUEENS]);

	while (att)
	{
		uint64 ray_to_att{ slide_att(ROOK, king_sq, att) };
		ray_to_att |= slide_att(BISHOP, king_sq, att);

		bb::bitscan(att);
		uint64 attacker{ 1ULL << bb::lsb() };

		if (!(attacker & ray_to_att))
		{
			att &= att - 1;
			continue;
		}

		assert(fr_king);

		uint64 x_ray{ 0 };
		for (int dir{ 0 }; dir < 8; ++dir)
		{
			auto flood{ fr_king };
			for (; !(flood & ray[dir].boarder); flood |= shift(flood, ray[dir].shift));

			if (flood & attacker)
			{
				x_ray = flood & ray_to_att;
				break;
			}
		}

		assert(x_ray & attacker);
		assert(!(x_ray & fr_king));

		// pinning all moves of pawns

		if ((x_ray & friends) && bb::popcnt(x_ray & board.side[BOTH]) == 2)
		{
			assert(bb::popcnt(x_ray & friends) == 1);

			bb::bitscan(x_ray & friends);
			pinned[bb::lsb()] = x_ray;
			pin_idx[pin_cnt++] = bb::lsb();
		}

		// pinning enpassant-moves of pawns

		else if (board.ep_square
			&& x_ray & friends & board.pieces[PAWNS]
			&& x_ray & enemies & board.pieces[PAWNS]
			&& bb::popcnt(x_ray & board.side[BOTH]) == 3)
		{
			assert(bb::popcnt(x_ray & enemies) == 2);

			uint64 enemy_pawn{ x_ray & enemies & board.pieces[PAWNS] };
			uint64 friend_pawn{ x_ray & friends & board.pieces[PAWNS] };

			if (friend_pawn << 1 == enemy_pawn || friend_pawn >> 1 == enemy_pawn)
			{
				if (board.ep_square == shift(enemy_pawn, push[board.turn]))
				{
					bb::bitscan(x_ray & friends);
					pinned[bb::lsb()] = ~board.ep_square;
					pin_idx[pin_cnt++] = bb::lsb();
				}
			}
		}

		att &= att - 1;
	}
}
void movegen::unpin()
{
	if (pin_cnt != 0)
	{
		assert(pin_cnt <= 8);
		for (int i{ 0 }; i < pin_cnt; ++i)
			pinned[pin_idx[i]] = 0xffffffffffffffff;
	}
}
void movegen::legal::pin_down(pos &board)
{
	// used by evaluation

	friends = board.side[board.turn];
	enemies = board.side[board.turn ^ 1];
	fr_king = board.pieces[KINGS] & friends;

	init_pin(board);
}

uint64 movegen::check(pos &board, int turn, uint64 squares)
{
	// returns the squares that are not under attack

	assert(turn == WHITE || turn == BLACK);

	const uint64 king{ board.side[turn] & board.pieces[KINGS] };
	uint64 inquire{ squares };

	while (inquire)
	{
		bb::bitscan(inquire);
		auto sq{ bb::lsb() };
		const uint64 sq64{ 1ULL << sq };
		const uint64 in_front[]{ ~(sq64 - 1), sq64 - 1 };

		uint64 att{ slide_att(ROOK, sq, board.side[BOTH] & ~king) & (board.pieces[ROOKS] | board.pieces[QUEENS]) };
		att |= slide_att(BISHOP, sq, board.side[BOTH] & ~king) & (board.pieces[BISHOPS] | board.pieces[QUEENS]);
		att |= knight_table[sq] & board.pieces[KNIGHTS];
		att |= king_table[sq] & board.pieces[KINGS];
		att |= king_table[sq] & board.pieces[PAWNS] & slide_ray[BISHOP][sq] & in_front[turn];
		att &= board.side[turn ^ 1];

		if (att) squares ^= sq64;

		inquire &= inquire - 1;
	}
	return squares;
}
uint64 movegen::slide_att(const int sl, const int sq, uint64 occ)
{
	assert(sq < 64 && sq >= 0);
	assert(sl == ROOK || sl == BISHOP);

	occ &= slider[sl][sq].mask;
	occ *= slider[sl][sq].magic;
	occ >>= slider[sl][sq].shift;
	return attack_table[slider[sl][sq].offset + occ];
}

uint16 *movegen::find(uint16 move)
{
	return std::find(list, list + move_cnt, move);
}
bool movegen::in_list(uint16 move)
{
	return find(move) != list + move_cnt;
}

// magic functions

void magic::init()
{
	attack_table.clear();
	attack_table.reserve(table_size);

	vector<uint64> attack_temp;
	attack_temp.reserve(table_size);

	vector<uint64> blocker;
	blocker.reserve(table_size);

	for (int sl{ ROOK }; sl <= BISHOP; ++sl)
	{
		init_mask(sl);
		init_blocker(sl, blocker);
		init_move(sl, blocker, attack_temp);
		init_magic(sl, blocker, attack_temp);
		init_connect(sl, blocker, attack_temp);
	}
}

void magic::init_mask(int sl)
{
	assert(sl == ROOK || sl == BISHOP);

	for (int sq{ 0 }; sq < 64; ++sq)
	{
		uint64 sq64{ 1ULL << sq };

		for (int dir{ sl }; dir < 8; dir += 2)
		{
			uint64 flood{ sq64 };
			while (!(flood & ray[dir].boarder))
			{
				slider[sl][sq].mask |= flood;
				real_shift(flood, ray[dir].shift);
			}
		}
		slider[sl][sq].mask ^= sq64;
	}
}
void magic::init_blocker(int sl, vector<uint64> &blocker)
{
	assert(sl == ROOK || sl == BISHOP);
	assert(blocker.size() == 0U || blocker.size() == 102400U);

	bool bit[12]{ false };

	for (int sq{ 0 }; sq < 64; ++sq)
	{
		slider[sl][sq].offset = blocker.size();

		uint64 mask_split[12]{ 0 };
		int bits_in{ 0 };

		uint64 mask_bit{ slider[sl][sq].mask };
		while (mask_bit)
		{
			bb::bitscan(mask_bit);
			mask_split[bits_in++] = 1ULL << bb::lsb();

			mask_bit &= mask_bit - 1;
		}
		assert(bits_in <= 12);
		assert(bits_in >= 5);
		assert(bb::popcnt(slider[sl][sq].mask) == bits_in);

		slider[sl][sq].shift = 64 - bits_in;

		int max{ 1 << bits_in };
		for (int a{ 0 }; a < max; ++a)
		{
			uint64 board{ 0 };
			for (int b{ 0 }; b < bits_in; ++b)
			{
				if (!(a % (1U << b)))
					bit[b] = !bit[b];
				if (bit[b])
					board |= mask_split[b];
			}
			blocker.push_back(board);
		}
	}
}
void magic::init_move(int sl, vector<uint64> &blocker, vector<uint64> &attack_temp)
{
	assert(sl == ROOK || sl == BISHOP);
	assert(attack_temp.size() == 0U || attack_temp.size() == 102400U);

	for (int sq{ 0 }; sq < 64; ++sq)
	{
		uint64 sq64{ 1ULL << sq };

		int max{ 1 << (64 - slider[sl][sq].shift) };
		for (int cnt{ 0 }; cnt < max; ++cnt)
		{
			uint64 board{ 0 };

			for (int dir{ sl }; dir < 8; dir += 2)
			{
				uint64 flood{ sq64 };
				while (!(flood & ray[dir].boarder) && !(flood & blocker[slider[sl][sq].offset + cnt]))
				{
					real_shift(flood, ray[dir].shift);
					board |= flood;
				}
			}
			attack_temp.push_back(board);

			assert(attack_temp.size() - 1 == slider[sl][sq].offset + cnt);
		}
	}
}
void magic::init_magic(int sl, vector<uint64> &blocker, vector<uint64> &attack_temp)
{
	const uint64 seeds[]
	{
		908859, 953436, 912753, 482262, 322368, 711868, 839234, 305746,
		711822, 703023, 270076, 964393, 704635, 626514, 970187, 398854
	};

	bool fail;
	for (int sq{ 0 }; sq < 64; ++sq)
	{
		vector<uint64> occ;

		int occ_size{ 1 << (64 - slider[sl][sq].shift) };
		occ.resize(occ_size);

		assert(occ.size() <= 4096U);
		assert(sq / 4 <= 16 && sq / 4 >= 0);

		rand_xor rand_gen{ seeds[sq / 4] };

		int max{ 1 << (64 - slider[sl][sq].shift) };

		do
		{
			do slider[sl][sq].magic = rand_gen.sparse64();
			while (bb::popcnt((slider[sl][sq].mask * slider[sl][sq].magic) & 0xff00000000000000) < 6);

			fail = false;
			occ.clear();
			occ.resize(occ_size);

			for (int i{ 0 }; !fail && i < max; ++i)
			{
				auto idx{ (blocker[slider[sl][sq].offset + i] * slider[sl][sq].magic) >> slider[sl][sq].shift };
				assert(idx <= static_cast<uint64>(occ_size));

				if (!occ[idx])
					occ[idx] = attack_temp[slider[sl][sq].offset + i];
				else if (occ[idx] != attack_temp[slider[sl][sq].offset + i])
				{
					fail = true;
					break;
				}
			}
		} while (fail);
	}
}
void magic::init_connect(int sl, vector<uint64> &blocker, vector<uint64> &attack_temp)
{
	assert(sl == ROOK || sl == BISHOP);
	assert(attack_table.size() == 0U || attack_table.size() == 102400U);

	for (int sq{ 0 }; sq < 64; ++sq)
	{
		int max{ 1 << (64 - slider[sl][sq].shift) };

		for (int cnt{ 0 }; cnt < max; ++cnt)
		{
			attack_table[slider[sl][sq].offset +
				((blocker[slider[sl][sq].offset + cnt] * slider[sl][sq].magic) >> slider[sl][sq].shift)]
				= attack_temp[slider[sl][sq].offset + cnt];
		}
	}
}

void magic::init_ray(int sl)
{
	assert(sl == ROOK || sl == BISHOP);

	for (int sq{ 0 }; sq < 64; ++sq)
	{
		uint64 sq64{ 1ULL << sq };

		for (int dir{ sl }; dir < 8; dir += 2)
		{
			uint64 flood{ sq64 };
			while (!(flood & ray[dir].boarder))
			{
				real_shift(flood, ray[dir].shift);
				slide_ray[sl][sq] |= flood;
			}
		}
	}
}
void magic::init_king()
{
	for (int sq{ 0 }; sq < 64; ++sq)
	{
		uint64 sq64{ 1ULL << sq };

		for (int dir{ 0 }; dir < 8; ++dir)
		{
			uint64 att{ sq64 };

			if (!(att & ray[dir].boarder))
				movegen::king_table[sq] |= shift(att, ray[dir].shift);
		}
	}
}
void magic::init_knight()
{
	const pattern jump[]
	{
		{ 15, 0xffff010101010101 },{  6, 0xff03030303030303 },
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

// attack table functions

uint64 attack::by_pawns(pos &board, int col)
{
	// returns all squares attacked by pawns of int color

	assert(col == WHITE || col == BLACK);

	uint64 att_table{ shift(board.pieces[PAWNS] & board.side[col] & ~boarder[col], cap_left[col]) };
	att_table |= shift(board.pieces[PAWNS] & board.side[col] & ~boarder[col ^ 1], cap_right[col]);
	
	return att_table;
}
