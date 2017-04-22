/*
  Monolith 0.1  Copyright (C) 2017 Jonas Mayr

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

namespace
{
	magic::magic_s slider[2][64];
	uint64 slide_ray[2][64];

	uint64 knight_table[64];
	uint64 king_table[64];

	const int table_size{ 107648 };
	vector<uint64> attack_table;

	inline void shift_real(uint64 &bb, int shift)
	{
		bb = (bb << shift) | (bb >> (64 - shift));
	}
	
	struct ray_s
	{
		int shift;
		uint64 boarder;
	};

	const ray_s ray[]
	{
		{ 8, 0xff00000000000000 }, { 7, 0xff01010101010101 },
		{ 63, 0x0101010101010101 }, { 55, 0x01010101010101ff },
		{ 56, 0x00000000000000ff }, { 57, 0x80808080808080ff },
		{ 1, 0x8080808080808080 }, { 9, 0xff80808080808080 }
	};

	const uint64 seeds[]
	{
		908859, 953436, 912753, 482262, 322368, 711868, 839234, 305746,
		711822, 703023, 270076, 964393, 704635, 626514, 970187, 398854
	};

	uint64 pinned[64];
	uint64 legal_sq;

	uint64 enemies, friends, king_fr, pawn_rank;
	uint64 _boarder_left, _boarder_right;
	uint64 gentype[2];

	uint32 sq_king_fr;

	uint8 AHEAD, BACK;

	uint32 pin_idx[8];
	int pin_cnt;

	const int cap_left[]{ 9, 55, -9 };
	const int cap_right[]{ 7, 57, -7 };
	const int push[]{ 8, 56, -8 };
	const int double_push[]{ 16, 48, -16 };

	const uint64 _file[]{ 0x7f7f7f7f7f7f7f7f, 0xfefefefefefefefe };
	const uint64 promo_rank{ 0xff000000000000ff };
	const uint64 sec_rank[]{ 0xff0000, 0xff0000000000 };
}

void movegen::init()
{
	for (auto &p : pinned) p = 0xffffffffffffffff;
}

int movegen::gen_moves(pos &board, gen_e type)
{
	AHEAD = board.turn;
	BACK = board.turn * 2;

	friends = board.side[board.turn];
	enemies = board.side[board.turn ^ 1];
	pawn_rank = sec_rank[board.turn];

	gentype[CAPTURES] = enemies;
	gentype[QUIETS] = ~board.side[BOTH];

	_boarder_right = _file[board.turn ^ 1];
	_boarder_left = _file[board.turn];

	king_fr = friends & board.pieces[KINGS];
	bb::bitscan(king_fr);
	sq_king_fr = bb::lsb();

	move_cnt = 0;
	pin_cnt = 0;

	legal(board);
	pin(board);

	king_moves(board, CAPTURES);
	pawn_promo(board);
	pawn_capt(board);
	piece_moves(board, CAPTURES);

	if (type == ALL)
	{
		pawn_quiet(board);
		piece_moves(board, QUIETS);
		king_moves(board, QUIETS);
	}

	unpin();
	return move_cnt;
}

void movegen::pawn_promo(pos &board)
{
	uint64 targets{ shift(board.pieces[PAWNS] & friends, push[AHEAD]) & ~board.side[BOTH] & legal_sq & promo_rank };
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
				movelist[move_cnt++] = encode(origin_sq, target_sq, flag);
		}

		targets &= targets - 1;
	}

	targets = shift(board.pieces[PAWNS] & friends & _boarder_left, cap_left[AHEAD]) & legal_sq & promo_rank & enemies;
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
				movelist[move_cnt++] = encode(origin_sq, target_sq, flag);
		}

		targets &= targets - 1;
	}

	targets = shift(board.pieces[PAWNS] & friends & _boarder_right, cap_right[AHEAD]) & legal_sq & promo_rank & enemies;
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
				movelist[move_cnt++] = encode(origin_sq, target_sq, flag);
		}

		targets &= targets - 1;
	}
}
void movegen::pawn_capt(pos &board)
{
	uint64 targets{ shift(board.pieces[PAWNS] & friends & _boarder_left, cap_left[AHEAD]) & ~promo_rank };
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
			movelist[move_cnt++] = encode(origin_sq, target_sq, board.piece_sq[target_sq]);

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
			movelist[move_cnt++] = encode(origin_sq, target_sq, ENPASSANT);
	}

	targets = shift(board.pieces[PAWNS] & friends & _boarder_right, cap_right[AHEAD]) & ~promo_rank;
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
			movelist[move_cnt++] = encode(origin_sq, target_sq, board.piece_sq[target_sq]);

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
			movelist[move_cnt++] = encode(origin_sq, target_sq, ENPASSANT);
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
			movelist[move_cnt++] = encode(origin_sq, target_sq, NONE);

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
			movelist[move_cnt++] = encode(origin_sq, target_sq, NONE);

		targets2x &= targets2x - 1;
	}
}
void movegen::piece_moves(pos &board, gen_e type)
{
	uint64 targets{ 0 };

	////queens
	uint64 pieces{ board.pieces[QUEENS] & friends };
	while (pieces)
	{
		bb::bitscan(pieces);
		auto queen{ bb::lsb() };
		targets = slide_att(bishop, queen, board.side[BOTH]) | slide_att(rook, queen, board.side[BOTH]);
		targets &= gentype[type] & legal_sq & pinned[queen];

		while (targets)
		{
			bb::bitscan(targets);
			movelist[move_cnt++] = encode(queen, bb::lsb(), board.piece_sq[bb::lsb()]);
			targets &= targets - 1;
		}
		pieces &= pieces - 1;
	}

	////knights
	pieces = board.pieces[KNIGHTS] & friends;
	while (pieces)
	{
		bb::bitscan(pieces);
		auto knight{ bb::lsb() };

		targets = knight_table[knight] & gentype[type] & legal_sq & pinned[knight];
		while (targets)
		{
			bb::bitscan(targets);
			movelist[move_cnt++] = encode(knight, bb::lsb(), board.piece_sq[bb::lsb()]);
			targets &= targets - 1;
		}
		pieces &= pieces - 1;
	}

	////bishops
	pieces = board.pieces[BISHOPS] & friends;
	while (pieces)
	{
		bb::bitscan(pieces);
		auto bishop_sq{ bb::lsb() };

		targets = slide_att(bishop, bishop_sq, board.side[BOTH]) & gentype[type] & legal_sq & pinned[bishop_sq];
		while (targets)
		{
			bb::bitscan(targets);
			movelist[move_cnt++] = encode(bishop_sq, bb::lsb(), board.piece_sq[bb::lsb()]);
			targets &= targets - 1;
		}
		pieces &= pieces - 1;
	}

	////rooks
	pieces = board.pieces[ROOKS] & friends;
	while (pieces)
	{
		bb::bitscan(pieces);
		auto rook_sq{ bb::lsb() };

		targets = slide_att(rook, rook_sq, board.side[BOTH]) & gentype[type] & legal_sq & pinned[rook_sq];
		while (targets)
		{
			bb::bitscan(targets);
			movelist[move_cnt++] = encode(rook_sq, bb::lsb(), board.piece_sq[bb::lsb()]);
			targets &= targets - 1;
		}
		pieces &= pieces - 1;
	}
}
void movegen::king_moves(pos &board, gen_e type)
{
	uint64 targets{ check(board, board.turn, king_table[sq_king_fr] & gentype[type]) };
	while (targets)
	{
		bb::bitscan(targets);
		movelist[move_cnt++] = encode(sq_king_fr, bb::lsb(), board.piece_sq[bb::lsb()]);
		targets &= targets - 1;
	}

	//// castling
	if (type == QUIETS && king_fr & 0x800000000000008)
	{
		const uint64 rank[]{ 0xff, 0xff00000000000000 };
		const uint64 rank_king{ rank[board.turn] };

		const uint8 rights_s[]{ 0x1, 0x10 };
		const uint8 rights_l[]{ 0x4, 0x40 };

		if ((rights_s[board.turn] & board.castl_rights)
			&& !(board.side[BOTH] & 0x600000000000006 & rank_king)
			&& bb::popcnt(check(board, board.turn, 0x0e0000000000000e & rank_king)) == 3)
			{
				const uint32 target[]{ 1, 57 };
				movelist[move_cnt++] = encode(sq_king_fr, target[board.turn], (castl_e::SHORT_WHITE + board.turn * 2));
			}
		if ((rights_l[board.turn] & board.castl_rights)
			&& !(board.side[BOTH] & 0x7000000000000070 & rank_king)
			&& bb::popcnt(check(board, board.turn, 0x3800000000000038 & rank_king)) == 3)
			{
				const uint32 target[]{ 5, 61 };
				movelist[move_cnt++] = encode(sq_king_fr, target[board.turn], (castl_e::LONG_WHITE + board.turn * 2));
			}
	}
}

void movegen::legal(pos &board)
{
	const uint64 side[]{ ~(king_fr - 1), king_fr - 1 };
	assert(king_fr != 0);

	uint64 attackers{ slide_att(rook, sq_king_fr, board.side[BOTH]) & (board.pieces[ROOKS] | board.pieces[QUEENS]) };
	attackers |= slide_att(bishop, sq_king_fr, board.side[BOTH]) & (board.pieces[BISHOPS] | board.pieces[QUEENS]);
	attackers |= knight_table[sq_king_fr] & board.pieces[KNIGHTS];
	attackers |= king_table[sq_king_fr] & board.pieces[PAWNS] & slide_ray[bishop][sq_king_fr] & side[board.turn];
	attackers &= enemies;

	int nr_att{ bb::popcnt(attackers) };

	if (nr_att == 0)
		legal_sq = 0xffffffffffffffff;
	else if (nr_att == 1)
	{
		if (attackers & board.pieces[KNIGHTS] || attackers & board.pieces[PAWNS])
			legal_sq = attackers;
		else
		{
			assert(attackers & board.pieces[ROOKS] || attackers & board.pieces[BISHOPS] || attackers & board.pieces[QUEENS]);
			
			uint64 single_ray[8]{ 0 };
			for (int dir{ 0 }; dir < 8; ++dir)
			{
				uint64 flood{ king_fr };
				while (!(flood & ray[dir].boarder))
				{
					shift_real(flood, ray[dir].shift);
					single_ray[dir] |= flood;
				}
			}

			for (auto &s : single_ray)
				if (s & attackers)
				{
					legal_sq = s & (slide_att(rook, sq_king_fr, board.side[BOTH]) | slide_att(bishop, sq_king_fr, board.side[BOTH]));
					break;
				}
		}
	}
	else
	{
		assert(nr_att == 2);
		legal_sq = 0ULL;
	}
}
void movegen::pin(pos &board)
{
	uint64 single_ray[8]{ 0 };

	uint64 attackers{ slide_ray[rook][sq_king_fr] & enemies & (board.pieces[ROOKS] | board.pieces[QUEENS]) };
	attackers |= slide_ray[bishop][sq_king_fr] & enemies & (board.pieces[BISHOPS] | board.pieces[QUEENS]);

	while (attackers)
	{
		uint64 rays_att{ slide_att(rook, sq_king_fr, attackers) };
		rays_att |= slide_att(bishop, sq_king_fr, attackers);

		uint64 pinning_ray{ 0 };

		bb::bitscan(attackers);
		uint64 attacker{ 1ULL << bb::lsb() };

		if (!(attacker & rays_att))
		{
			attackers &= attackers - 1;
			continue;
		}

		assert(king_fr != 0);

		if (std::all_of(single_ray, single_ray + 8, [](uint64 i) {return i == 0ULL; }))
		{
			for (int dir{ 0 }; dir < 8; ++dir)
			{
				uint64 flood{ king_fr };
				while (!(flood & ray[dir].boarder))
				{
					shift_real(flood, ray[dir].shift);
					single_ray[dir] |= flood;
				}
			}
		}

		for (auto &s : single_ray)
			if (s & attacker)
			{
				pinning_ray = s & rays_att;
				break;
			}

		assert(pinning_ray & attacker);

		if ((pinning_ray & friends) && bb::popcnt(pinning_ray & board.side[BOTH]) == 2)
		{
			assert(bb::popcnt(pinning_ray & friends) == 1);

			bb::bitscan(pinning_ray & friends);
			pinned[bb::lsb()] = pinning_ray;
			pin_idx[pin_cnt++] = bb::lsb();
		}

		//// pinning enpassant-moves of pawns
		else if (board.ep_square
			&& pinning_ray & friends & board.pieces[PAWNS]
			&& pinning_ray & enemies & board.pieces[PAWNS]
			&& bb::popcnt(pinning_ray & board.side[BOTH]) == 3)
		{
			assert(bb::popcnt(pinning_ray & enemies) == 2);
			uint64 enemy_pawn{ pinning_ray & enemies & board.pieces[PAWNS] };

			if ((pinning_ray & friends & board.pieces[PAWNS]) << 1 == (pinning_ray & enemies & board.pieces[PAWNS])
				|| (pinning_ray & friends & board.pieces[PAWNS]) >> 1 == (pinning_ray & enemies & board.pieces[PAWNS]))
			{
				assert(bb::popcnt(pinning_ray & enemies & board.pieces[PAWNS]) == 1);

				shift_real(enemy_pawn, push[board.turn]);

				if (board.ep_square == enemy_pawn)
				{
					bb::bitscan(pinning_ray & friends);
					pinned[bb::lsb()] = ~enemy_pawn;
					pin_idx[pin_cnt++] = bb::lsb();
				}
			}
		}
		attackers &= attackers - 1;
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
uint64 movegen::check(pos &board, int turn, uint64 squares)
{
	//// returns the squares that are not under attack

	assert(turn == white || turn == black);

	const uint64 king{ board.side[turn] & board.pieces[KINGS] };
	uint64 enemy{ board.side[turn ^ 1] };
	uint64 inquire{ squares };

	while (inquire)
	{
		bb::bitscan(inquire);
		auto sq{ bb::lsb() };
		const uint64 sq64{ 1ULL << sq };
		const uint64 side[]{ ~(sq64 - 1), sq64 - 1 };

		uint64 attacker{ slide_att(rook, sq, board.side[BOTH] & ~king) & (board.pieces[ROOKS] | board.pieces[QUEENS]) };
		attacker |= slide_att(bishop, sq, board.side[BOTH] & ~king) & (board.pieces[BISHOPS] | board.pieces[QUEENS]);
		attacker |= knight_table[sq] & board.pieces[KNIGHTS];
		attacker |= king_table[sq] & board.pieces[KINGS];
		attacker |= king_table[sq] & board.pieces[PAWNS] & slide_ray[bishop][sq] & side[turn];
		attacker &= enemy;

		if (attacker)
			squares ^= sq64;
		inquire &= inquire - 1;
	}
	return squares;
}

uint64 movegen::slide_att(const uint8 sl, const int sq, uint64 occ)
{
	assert(sq < 64 && sq >= 0);
	assert(sl == rook || sl == bishop);

	occ &= slider[sl][sq].mask;
	occ *= slider[sl][sq].magic;
	occ >>= slider[sl][sq].shift;
	return attack_table[slider[sl][sq].offset + occ];
}

uint16 *movegen::find(uint16 move)
{
	return std::find(movelist, movelist + move_cnt, move);
}
bool movegen::in_list(uint16 move)
{
	return find(move) != movelist + move_cnt;
}

void magic::init()
{
	attack_table.clear();
	attack_table.reserve(table_size);

	vector<uint64> attack_temp;
	attack_temp.reserve(table_size);

	vector<uint64> blocker;
	blocker.reserve(table_size);

	for (uint8 sl{ rook }; sl <= bishop; ++sl)
	{
		init_mask(sl);
		init_blocker(sl, blocker);
		init_move(sl, blocker, attack_temp);
		init_magic(sl, blocker, attack_temp);
		init_connect(sl, blocker, attack_temp);
	}
}
void magic::init_mask(uint8 sl)
{
	assert(sl == rook || sl == bishop);

	for (int sq{ 0 }; sq < 64; ++sq)
	{
		uint64 square{ 1ULL << sq };
		assert(slider[sl][sq].mask == 0ULL);

		for (int dir{ sl }; dir < 8; dir += 2)
		{
			uint64 flood{ square };
			while (!(flood & ray[dir].boarder))
			{
				slider[sl][sq].mask |= flood;
				shift_real(flood, ray[dir].shift);
			}
		}
		slider[sl][sq].mask ^= square;
	}
}
void magic::init_blocker(uint8 sl, vector<uint64> &blocker)
{
	bool bit[12]{ false };

	assert(blocker.size() == 0U || blocker.size() == 102400U);

	for (int sq{ 0 }; sq < 64; ++sq)
	{
		assert(slider[sl][sq].offset == 0);

		slider[sl][sq].offset = blocker.size();

		uint64 mask_split[12]{};
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
		assert(slider[sl][sq].shift == 0);

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
void magic::init_move(uint8 sl, vector<uint64> &blocker, vector<uint64> &attack_temp)
{
	assert(sl == rook || sl == bishop);
	assert(attack_temp.size() == 0U || attack_temp.size() == 102400U);

	for (int sq{ 0 }; sq < 64; ++sq)
	{
		uint64 square{ 1ULL << sq };

		int max{ 1 << (64 - slider[sl][sq].shift) };
		for (int cnt{ 0 }; cnt < max; ++cnt)
		{
			uint64 board{ 0 };

			for (int dir{ sl }; dir < 8; dir += 2)
			{
				uint64 flood{ square };
				while (!(flood & ray[dir].boarder) && !(flood & blocker[slider[sl][sq].offset + cnt]))
				{
					shift_real(flood, ray[dir].shift);
					board |= flood;
				}
			}
			attack_temp.push_back(board);

			assert(attack_temp.size() - 1 == slider[sl][sq].offset + cnt);
		}
	}
}
void magic::init_magic(uint8 sl, vector<uint64> &blocker, vector<uint64> &attack_temp)
{
	bool fail;
	for (int sq{ 0 }; sq < 64; ++sq)
	{
		vector<uint64> occ;

		int SIZE_occ{ 1 << (64 - slider[sl][sq].shift) };
		occ.resize(SIZE_occ);
		assert(occ.size() <= 4096U);

		assert(static_cast<int>(sq / 4) <= 16);
		assert(static_cast<int>(sq / 4) >= 0);

		rand_xor rand_gen{ seeds[static_cast<int>(sq / 4)] };

		int max{ 1 << (64 - slider[sl][sq].shift) };

		assert(slider[sl][sq].magic == 0ULL);
		do
		{
			do slider[sl][sq].magic = rand_gen.sparse64();
			while (bb::popcnt((slider[sl][sq].mask * slider[sl][sq].magic) & 0xff00000000000000) < 6);

			fail = false;
			occ.clear();
			occ.resize(SIZE_occ);

			for (int i{ 0 }; !fail && i < max; ++i)
			{
				int index{ static_cast<int>((blocker[slider[sl][sq].offset + i] * slider[sl][sq].magic) >> slider[sl][sq].shift) };

				if (!occ[index])
					occ[index] = attack_temp[slider[sl][sq].offset + i];
				else if (occ[index] != attack_temp[slider[sl][sq].offset + i])
				{
					fail = true;
					break;
				}
			}
		} while (fail);
	}
}
void magic::init_connect(uint8 sl, vector<uint64> &blocker, vector<uint64> &attack_temp)
{
	assert(sl == rook || sl == bishop);
	assert(attack_table.size() == 0U || attack_table.size() == 102400U);

	for (int sq{ 0 }; sq < 64; ++sq)
	{
		int max{ 1 << (64 - slider[sl][sq].shift) };

		for (int cnt{ 0 }; cnt < max; ++cnt)
			attack_table[slider[sl][sq].offset +
			(static_cast<int>((blocker[slider[sl][sq].offset + cnt] * slider[sl][sq].magic)
				>> slider[sl][sq].shift))]
			= attack_temp[slider[sl][sq].offset + cnt];
	}
}

void magic::init_ray(uint8 sl)
{
	assert(sl == rook || sl == bishop);

	for (int sq{ 0 }; sq < 64; ++sq)
	{
		uint64 square{ 1ULL << sq };
		assert(slide_ray[sl][sq] == 0ULL);

		for (int dir{ sl }; dir < 8; dir += 2)
		{
			uint64 flood{ square };
			while (!(flood & ray[dir].boarder))
			{
				shift_real(flood, ray[dir].shift);
				slide_ray[sl][sq] |= flood;
			}
		}
	}
}
void magic::init_king()
{
	for (int sq{ 0 }; sq < 64; ++sq)
	{
		uint64 square{ 1ULL << sq };
		assert(king_table[sq] == 0ULL);

		for (int dir{ 0 }; dir < 8; ++dir)
		{
			uint64 att{ square };
			if (!(att & ray[dir].boarder))
			{
				king_table[sq] |= shift(att, ray[dir].shift);
			}
		}
	}
}
void magic::init_knight()
{
	const ray_s pattern[]
	{
		{ 15, 0xffff010101010101 },{ 6, 0xff03030303030303 },
		{ 54, 0x03030303030303ff },{ 47, 0x010101010101ffff },
		{ 49, 0x808080808080ffff },{ 58, 0xc0c0c0c0c0c0c0ff },
		{ 10, 0xffc0c0c0c0c0c0c0 },{ 17, 0xffff808080808080 }
	};

	for (int sq{ 0 }; sq < 64; ++sq)
	{
		uint64 square{ 1ULL << sq };
		assert(knight_table[sq] == 0ULL);

		for (int dir{ 0 }; dir < 8; ++dir)
		{
			uint64 att{ square };
			if (!(att & pattern[dir].boarder))
			{
				knight_table[sq] |= shift(att, pattern[dir].shift);
			}
		}
	}
}