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


#include "hash.h"
#include "bitboard.h"
#include "position.h"

namespace
{
	const char piece_char[][6]
	{
		{ 'P', 'N', 'B', 'R', 'Q', 'K' },
		{ 'p', 'n', 'b', 'r', 'q', 'k' }
	};

	const char castling_char[] { 'K', 'k', 'Q', 'q' };

	const int push[]{ 8, 56 };

	const int phase_value[]{ 0, 2, 2, 3, 7, 0 };

	const int reorder[]{ 0, 2, 1, 3 };
	
	int mirror(uint16 sq)
	{
		return (sq & 56) - (sq & 7) + 7;
	}
}

void pos::new_move(uint32 move)
{
	move_cnt += 1;
	half_move_cnt += 1;
	capture = 0;

	move_detail md{ decode(move) };

	uint64 sq1_64{ 1ULL << md.sq1 };
	uint64 sq2_64{ 1ULL << md.sq2 };

	assert(md.sq1 >= 0 && md.sq1 < 64);
	assert(md.sq2 >= 0 && md.sq2 < 64);
	assert(md.piece != NONE);
	assert(md.piece == piece_sq[md.sq1]);
	assert(md.victim == piece_sq[md.sq2] || md.flag == ENPASSANT);

	// preventing castling

	bool s_castl_rights[4];
	for (int i{ 0 }; i < 4; ++i)
		s_castl_rights[i] = castl_rights[i];

	rook_moved(sq2_64, md.sq2);
	rook_moved(sq1_64, md.sq1);

	// deleting the eventually captured piece

	if (md.victim != NONE)
	{
		// enpassant capturing

		if (md.flag == ENPASSANT)
		{
			assert(ep_sq != 0);

			uint64 capt{ shift(ep_sq, push[not_turn]) };
			assert(capt & pieces[PAWNS] & side[not_turn]);

			pieces[PAWNS] &= ~capt;
			side[not_turn] &= ~capt;

			uint16 sq_old{ static_cast<uint16>(bb::bitscan(capt)) };

			assert(piece_sq[sq_old] == PAWNS);
			piece_sq[sq_old] = NONE;

			capture = md.sq2;
			key ^= zobrist::rand_key[(turn << 6) + mirror(sq_old)];
		}

		// normal capturing
		
		else
		{
			assert(sq2_64 & side[not_turn]);
			half_move_cnt = 0;

			side[not_turn] &= ~sq2_64;
			pieces[md.victim] &= ~sq2_64;

			capture = md.sq2;
			phase -= phase_value[md.victim];
			key ^= zobrist::rand_key[(((md.victim << 1) + turn) << 6) + mirror(md.sq2)];

			assert(phase >= 0);
		}
	}

	// resetting the half-move-clock

	else if (md.piece == PAWNS)
	{
		half_move_cnt = 0;
	}

	// enpassant square is not valid anymore

	if (ep_sq)
	{
		auto file_idx{ bb::bitscan(ep_sq) & 7 };
		if (pieces[PAWNS] & side[turn] & zobrist::ep_flank[not_turn][file_idx])
			key ^= zobrist::rand_key[zobrist::offset.ep + 7 - file_idx];

		ep_sq = 0ULL;
	}

	// doublepushing pawn

	if (md.flag == DOUBLEPUSH)
	{
		assert(md.piece == PAWNS && md.victim == NONE);
		ep_sq = 1ULL << ((md.sq1 + md.sq2) / 2);

		auto file_idx{ md.sq1 & 7 };
		if (pieces[PAWNS] & side[not_turn] & zobrist::ep_flank[turn][file_idx])
			key ^= zobrist::rand_key[zobrist::offset.ep + 7 - file_idx];
	}

	// doing the move

	pieces[md.piece] ^= sq1_64;
	pieces[md.piece] |= sq2_64;

	side[turn] ^= sq1_64;
	side[turn] |= sq2_64;

	piece_sq[md.sq2] = md.piece;
	piece_sq[md.sq1] = NONE;

	int idx{ ((md.piece << 1) + not_turn) << 6 };
	key ^= zobrist::rand_key[idx + mirror(md.sq1)];
	key ^= zobrist::rand_key[idx + mirror(md.sq2)];

	if (md.flag >= 8)
	{
		// castling

		if (md.flag <= 11)
		{
			switch (md.flag)
			{
			case CASTLING::WHITE_SHORT:
				pieces[ROOKS] ^= 0x1, side[turn] ^= 0x1;
				pieces[ROOKS] |= 0x4, side[turn] |= 0x4;
				piece_sq[H1] = NONE, piece_sq[F1] = ROOKS;
				
				key ^= zobrist::rand_key[((7 - turn) << 6) + 7];
				key ^= zobrist::rand_key[((7 - turn) << 6) + 5];
				break;
				
			case CASTLING::BLACK_SHORT:
				pieces[ROOKS] ^= 0x100000000000000, side[turn] ^= 0x100000000000000;
				pieces[ROOKS] |= 0x400000000000000, side[turn] |= 0x400000000000000;
				piece_sq[H8] = NONE, piece_sq[F8] = ROOKS;

				key ^= zobrist::rand_key[((7 - turn) << 6) + 63];
				key ^= zobrist::rand_key[((7 - turn) << 6) + 61];
				break;
				
			case CASTLING::WHITE_LONG:
				pieces[ROOKS] ^= 0x80, side[turn] ^= 0x80;
				pieces[ROOKS] |= 0x10, side[turn] |= 0x10;
				piece_sq[A1] = NONE, piece_sq[D1] = ROOKS;

				key ^= zobrist::rand_key[ (7 - turn) << 6];
				key ^= zobrist::rand_key[((7 - turn) << 6) + 3];
				break;
				
			case CASTLING::BLACK_LONG:
				pieces[ROOKS] ^= 0x8000000000000000, side[turn] ^= 0x8000000000000000;
				pieces[ROOKS] |= 0x1000000000000000, side[turn] |= 0x1000000000000000;
				piece_sq[A8] = NONE, piece_sq[D8] = ROOKS;

				key ^= zobrist::rand_key[((7 - turn) << 6) + 56];
				key ^= zobrist::rand_key[((7 - turn) << 6) + 59];
				break;
				
			default:
				assert(false);
			}
		}

		// promoting

		else
		{
			int promo_p{ md.flag - 11 };

			assert(md.flag >= 12 && md.flag <= 15);
			assert(pieces[PAWNS] & sq2_64);
			assert(piece_sq[md.sq2] == PAWNS);

			pieces[PAWNS] ^= sq2_64;
			pieces[promo_p] |= sq2_64;
			piece_sq[md.sq2] = promo_p;

			int sq_new{ mirror(md.sq2) };
			key ^= zobrist::rand_key[(not_turn << 6) + sq_new];
			key ^= zobrist::rand_key[(((promo_p << 1) + not_turn) << 6) + sq_new];

			phase += phase_value[promo_p];
		}
	}

	// preventing castling when king is moved

	if (sq2_64 & pieces[KINGS])
	{
		castl_rights[turn] = false;
		castl_rights[turn + 2] = false;

		king_sq[turn] = md.sq2;
	}

	// updating the hash when castling rights change

	for (int i{ 0 }; i < 4; ++i)
	{
		if(s_castl_rights[i] != castl_rights[i])
			key ^= zobrist::rand_key[zobrist::offset.castling + reorder[i]];
	}

	// updating side to move

	not_turn ^= 1;
	turn ^= 1;
	key ^= zobrist::is_turn[0];

	side[BOTH] = side[WHITE] | side[BLACK];

	assert(turn == (not_turn ^ 1));
	assert(side[BOTH] == (side[WHITE] ^ side[BLACK]));
	ASSERT(zobrist::to_key(*this) == key);
}

void pos::rook_moved(uint64 &sq64, uint16 sq)
{
	// updating castling rights when rook is moved

	if (sq64 & pieces[ROOKS])
	{
		if (sq64 & side[WHITE])
		{
			if (sq == H1) castl_rights[WS] = false;
			else if (sq == A1) castl_rights[WL] = false;
		}
		else
		{
			if (sq == H8) castl_rights[BS] = false;
			else if (sq == A8) castl_rights[BL] = false;
		}
	}
}

void pos::null_move(uint64 &ep_copy, uint16 &capt_copy)
{
	// doing a null move, used for null move pruning

	key ^= zobrist::is_turn[0];
	capt_copy = capture;

	if (ep_sq)
	{
		auto file_idx{ bb::bitscan(ep_sq) & 7 };
		if (pieces[PAWNS] & side[turn] & zobrist::ep_flank[not_turn][file_idx])
			key ^= zobrist::rand_key[zobrist::offset.ep + 7 - file_idx];

		ep_copy = ep_sq;
		ep_sq = 0;
	}

	half_move_cnt += 1;
	move_cnt += 1;
	turn ^= 1;
	not_turn ^= 1;

	ASSERT(zobrist::to_key(*this) == key);
}

void pos::undo_null_move(uint64 &ep_copy, uint16 &capt_copy)
{
	// undoing the null move

	key ^= zobrist::is_turn[0];
	capture = capt_copy;

	if (ep_copy)
	{
		auto file_idx{ bb::bitscan(ep_copy) & 7 };
		if (pieces[PAWNS] & side[not_turn] & zobrist::ep_flank[turn][file_idx])
			key ^= zobrist::rand_key[zobrist::offset.ep + 7 - file_idx];

		ep_sq = ep_copy;
	}

	half_move_cnt -= 1;
	move_cnt -= 1;
	turn ^= 1;
	not_turn ^= 1;

	ASSERT(zobrist::to_key(*this) == key);
}

void pos::clear()
{
	for (auto &p : pieces) p = 0ULL;
	for (auto &s : side) s = 0ULL;
	for (auto &p : piece_sq) p = NONE;
	for (auto &c : castl_rights) c = false;

	move_cnt = 0;
	half_move_cnt = 0;
	ep_sq = 0ULL;
	turn = WHITE;
	not_turn = BLACK;
	phase = 0;
	capture = 0;
}

void pos::parse_fen(std::string fen)
{
	clear();
	int sq{ 63 };
	uint32 focus{ 0 };
	assert(focus < fen.size());

	// all pieces

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
					if (fen[focus] == piece_char[col][piece])
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

	// finding king squares

	for (int col{ WHITE }; col <= BLACK; ++col)
		king_sq[col] = bb::bitscan(pieces[KINGS] & side[col]);
	
	// side to move

	focus += 1;
	if (fen[focus] == 'w')
		turn = WHITE;
	else if (fen[focus] == 'b')
		turn = BLACK;
	not_turn = turn ^ 1;

	// castling rights

	focus += 2;
	while (focus < fen.size() && fen[focus] != ' ')
	{
		for (int i{ 0 }; i < 4; ++i)
		{
			if (fen[focus] == castling_char[i])
				castl_rights[i] = true;
		}
		focus += 1;
	}

	// enpassant possibility

	focus += 1;
	if (fen[focus] == '-')
		focus += 1;
	else
	{
		ep_sq = to_bb(fen.substr(focus, 2));
		focus += 2;
	}

	key = zobrist::to_key(*this);

	if (focus >= fen.size() - 1)
		return;

	// half move count

	focus += 1;
	std::string half_moves;
	while (focus < fen.size() && fen[focus] != ' ')
		half_moves += fen[focus++];
	half_move_cnt = stoi(half_moves);

	// move count

	focus += 1;
	std::string moves;
	while (focus < fen.size() && fen[focus] != ' ')
		moves += fen[focus++];
	move_cnt = stoi(moves) * 2 - 1 - turn;
}

bool pos::lone_king() const
{
	return (pieces[KINGS] | pieces[PAWNS]) == side[BOTH];
}

bool pos::recapture(uint32 move) const
{
	return to_sq2(move) == capture;
}
