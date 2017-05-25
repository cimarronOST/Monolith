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


#include "hash.h"
#include "convert.h"
#include "bitboard.h"
#include "position.h"

uint64 pos::nodes;

namespace
{
	const char p_char[][6]
	{
		{ 'P', 'R', 'N', 'B', 'Q', 'K' },
		{ 'p', 'r', 'n', 'b', 'q', 'k' }
	};

	const uint8 castl_r[]{ 0xfa, 0xaf };
	const int push[]{ 8, 56 };

	const int phase_value[]{ 0, 1, 1, 2, 4, 0, 0 };
	
	int mirror(uint16 sq)
	{
		return (sq & 56) - (sq & 7) + 7;
	}
}

void pos::new_move(uint16 move)
{
	nodes += 1;
	moves += 1;
	half_moves += 1;

	auto sq1{ to_sq1(move) };
	auto sq2{ to_sq2(move) };
	assert(sq1 >= 0 && sq1 < 64);
	assert(sq2 >= 0 && sq2 < 64);

	uint64 sq1_64{ 1ULL << sq1 };
	uint64 sq2_64{ 1ULL << sq2 };

	auto flag{ to_flag(move) };
	auto piece{ piece_sq[sq1] };
	auto victim{ piece_sq[sq2] };

	assert(flag == victim || flag >= 7);
	assert(piece != NONE);

	// preventing castling

	auto saved_castl_r{ castl_rights };
	rook_moved(sq2_64, sq2);
	rook_moved(sq1_64, sq1);

	// deleting the eventually captured piece

	if (victim != NONE)
	{
		assert(sq2_64 & side[turn ^ 1]);
		half_moves = 0;

		side[turn ^ 1] &= ~sq2_64;
		pieces[victim] &= ~sq2_64;

		phase -= phase_value[victim];
		key ^= hashing::random64[(hashing::piece_12[victim] - turn) * 64 + mirror(sq2)];

		assert(phase >= 0);
	}
	else if (piece == PAWNS)
	{
		half_moves = 0;
		if (flag == ENPASSANT)
		{
			assert(ep_square != 0);

			uint64 capt{ shift(ep_square, push[turn ^ 1]) };
			assert(capt & pieces[PAWNS] & side[turn ^ 1]);

			pieces[PAWNS] &= ~capt;
			side[turn ^ 1] &= ~capt;

			bb::bitscan(capt);
			uint16 sq_old{ static_cast<uint16>(bb::lsb()) };
			
			assert(piece_sq[sq_old] == PAWNS);
			piece_sq[sq_old] = NONE;

			key ^= hashing::random64[(BLACK - turn) * 64 + mirror(sq_old)];
		}
	}

	// pawn double push

	ep_square = 0ULL;
	if (piece == PAWNS && abs(sq1 - sq2) == 16)
	{
		ep_square = shift(sq1_64, push[turn]);

		auto file_idx{ sq1 & 7 };
		if (pieces[PAWNS] & side[turn ^ 1] & hashing::ep_flank[turn][file_idx])
			key ^= hashing::random64[hashing::offset.ep + 7 - file_idx];
	}

	// doing the move

	pieces[piece] ^= sq1_64; side[turn] ^= sq1_64;
	pieces[piece] |= sq2_64; side[turn] |= sq2_64;
	piece_sq[sq2] = piece;
	piece_sq[sq1] = NONE;

	int piece12 = hashing::piece_12[piece];
	key ^= hashing::random64[(piece12 - turn) * 64 + mirror(sq1)];
	key ^= hashing::random64[(piece12 - turn) * 64 + mirror(sq2)];

	if (flag >= 8)
	{
		// castling

		if (flag <= 11)
		{
			switch (flag)
			{
			case castl_e::SHORT_WHITE:
				pieces[ROOKS] ^= 0x1, side[turn] ^= 0x1;
				pieces[ROOKS] |= 0x4, side[turn] |= 0x4;
				piece_sq[H1] = NONE, piece_sq[F1] = ROOKS;
				
				key ^= hashing::random64[(7 - turn) * 64 + 7];
				key ^= hashing::random64[(7 - turn) * 64 + 5];
				break;
				
			case castl_e::SHORT_BLACK:
				pieces[ROOKS] ^= 0x100000000000000, side[turn] ^= 0x100000000000000;
				pieces[ROOKS] |= 0x400000000000000, side[turn] |= 0x400000000000000;
				piece_sq[H8] = NONE, piece_sq[F8] = ROOKS;

				key ^= hashing::random64[(7 - turn) * 64 + 63];
				key ^= hashing::random64[(7 - turn) * 64 + 61];
				break;
				
			case castl_e::LONG_WHITE:
				pieces[ROOKS] ^= 0x80, side[turn] ^= 0x80;
				pieces[ROOKS] |= 0x10, side[turn] |= 0x10;
				piece_sq[A1] = NONE, piece_sq[D1] = ROOKS;

				key ^= hashing::random64[(7 - turn) * 64];
				key ^= hashing::random64[(7 - turn) * 64 + 3];
				break;
				
			case castl_e::LONG_BLACK:
				pieces[ROOKS] ^= 0x8000000000000000, side[turn] ^= 0x8000000000000000;
				pieces[ROOKS] |= 0x1000000000000000, side[turn] |= 0x1000000000000000;
				piece_sq[A8] = NONE, piece_sq[D8] = ROOKS;

				key ^= hashing::random64[(7 - turn) * 64 + 56];
				key ^= hashing::random64[(7 - turn) * 64 + 59];
				break;
				
			default:
				assert(false);
			}
		}

		// promotion

		else
		{
			int promo_p{ flag - 11 };

			assert(flag >= 12 && flag <= 15);
			assert(pieces[PAWNS] & sq2_64);
			assert(piece_sq[sq2] == PAWNS);

			pieces[PAWNS] ^= sq2_64;
			pieces[promo_p] |= sq2_64;
			piece_sq[sq2] = promo_p;

			int sq_new{ mirror(sq2) };
			key ^= hashing::random64[(BLACK - turn) * 64 + sq_new];
			key ^= hashing::random64[(hashing::piece_12[promo_p] - turn) * 64 + sq_new];

			phase += phase_value[promo_p];
		}
	}

	// preventing castling when king is moved

	if (sq2_64 & pieces[KINGS])
	{
		castl_rights &= castl_r[turn];
		king_sq[turn] = sq2;
	}

	if (saved_castl_r != castl_rights)
	{
		auto changes{ saved_castl_r ^ castl_rights };
		for (int i{ 0 }; i < 4; ++i)
		{
			if (changes & castl_right[i])
				key ^= hashing::random64[hashing::offset.castl + i];
		}
	}

	turn ^= 1;
	key ^= hashing::is_turn[0];

	side[BOTH] = side[WHITE] | side[BLACK];
	assert(side[BOTH] == (side[WHITE] ^ side[BLACK]));
}

void pos::null_move(uint64 &ep_copy)
{
	// updating hash key

	key ^= hashing::is_turn[0];
	if (ep_square != 0)
	{
		bb::bitscan(ep_square);
		auto file_idx{ bb::lsb() & 7 };
		if (pieces[PAWNS] & side[turn ^ 1] & hashing::ep_flank[turn][file_idx])
			key ^= hashing::random64[hashing::offset.ep + 7 - file_idx];
	}

	ep_copy = ep_square;
	ep_square = 0;
	half_moves += 1;
	moves += 1;
	nodes += 1;
	turn ^= 1;
}
void pos::undo_null_move(uint64 &ep_copy)
{
	// updating hash key

	key ^= hashing::is_turn[0];
	if (ep_copy != 0)
	{
		bb::bitscan(ep_copy);
		auto file_idx{ bb::lsb() & 7 };
		if (pieces[PAWNS] & side[turn ^ 1] & hashing::ep_flank[turn][file_idx])
			key ^= hashing::random64[hashing::offset.ep + 7 - file_idx];
	}

	ep_square = ep_copy;
	half_moves -= 1;
	moves -= 1;
	turn ^= 1;
}
void pos::clear()
{
	for (auto &p : pieces) p = 0ULL;
	for (auto &s : side) s = 0ULL;
	for (auto &p : piece_sq) p = NONE;

	moves = 0;
	half_moves = 0;
	castl_rights = 0;
	ep_square = 0ULL;
	turn = WHITE;
	phase = 0;
}
void pos::parse_fen(string fen)
{
	clear();
	int sq{ 63 };
	uint32 focus{ 0 };
	assert(focus < fen.size());

	while (focus < fen.size() && fen[focus] != ' ')
	{
		assert(sq >= 0);

		if (fen[focus] == '/')
		{
			focus += 1;
			assert(focus < fen.size());
			continue;
		}
		else if (isdigit(static_cast<unsigned>(fen[focus])))
		{
			sq -= fen[focus] - '0';
			assert(fen[focus] - '0' <= 8 && fen[focus] - '0' >= 1);
		}
		else
		{
			for (int piece{ PAWNS }; piece <= KINGS; ++piece)
			{
				for (int col{ WHITE }; col <= BLACK; ++col)
				{
					if (fen[focus] == p_char[col][piece])
					{
						pieces[piece] |= 1ULL << sq;
						side[col] |= 1ULL << sq;
						piece_sq[sq] = piece;

						phase += phase_value[piece];
						break;
					}
				}
			}
			sq -= 1;
		}
		focus += 1;
		assert(focus < fen.size());
	}

	side[BOTH] = side[WHITE] | side[BLACK];
	assert(side[BOTH] == (side[WHITE] ^ side[BLACK]));

	// king squares

	for (int col{ WHITE }; col <= BLACK; ++col)
	{
		bb::bitscan(pieces[KINGS] & side[col]);
		king_sq[col] = bb::lsb();
	}
	
	// turn

	focus += 1;
	if (fen[focus] == 'w')
		turn = WHITE;
	else if (fen[focus] == 'b')
		turn = BLACK;
	
	// castling

	focus += 2;
	while (focus < fen.size() && fen[focus] != ' ')
	{
		if (fen[focus] == '-')
			;
		else if (fen[focus] == 'K')
			castl_rights |= castl_right[SW];
		else if (fen[focus] == 'Q')
			castl_rights |= castl_right[LW];
		else if (fen[focus] == 'k')
			castl_rights |= castl_right[SB];
		else if (fen[focus] == 'q')
			castl_rights |= castl_right[LB];
		focus += 1;
	}

	// enpassant

	focus += 1;
	if (fen[focus] == '-')
		focus += 1;
	else
	{
		ep_square = conv::to_bb(fen.substr(focus, 2));
		focus += 2;
	}

	if (focus >= fen.size() - 1)
		return;

	// half move count

	focus += 1;
	string half_move_c;
	while (focus < fen.size() && fen[focus] != ' ')
		half_move_c += fen[focus++];
	half_moves = stoi(half_move_c);

	// move count

	focus += 1;
	string move_c;
	while (focus < fen.size() && fen[focus] != ' ')
		move_c += fen[focus++];
	moves = stoi(move_c);

	key = hashing::to_key(*this);
}

void pos::rook_moved(uint64 &sq64, uint16 sq)
{
	if (sq64 & pieces[ROOKS])
	{
		if (sq64 & side[WHITE])
		{
			if (sq == H1) castl_rights &= ~castl_right[SW];
			else if (sq == A1) castl_rights &= ~castl_right[LW];
		}
		else
		{
			if (sq == H8) castl_rights &= ~castl_right[SB];
			else if (sq == A8) castl_rights &= ~castl_right[LB];
		}
	}
}

bool pos::lone_king() const
{
	return (pieces[KINGS] | pieces[PAWNS]) == side[BOTH];
}
