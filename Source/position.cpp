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


#include <sstream>

#include "move.h"
#include "pawn.h"
#include "trans.h"
#include "attack.h"
#include "zobrist.h"
#include "bit.h"
#include "position.h"

namespace
{
	// constants for fast indexing

	const int push[]
	{ 8, 56 };
	const uint8 king_target[][2]
	{ { G1, C1 }, { G8, C8 } };
	const uint8 rook_target[][2]
	{ { F1, D1 }, { F8, D8 } };
}

namespace square
{
	int index(std::string &sq)
	{
		assert(sq.size() == 2);
		return 'h' - sq.front() + ((sq.back() - '1') << 3);
	}
}

void board::new_move(uint32 move)
{
	// updating the board with a new move

	move_cnt += 1;
	half_move_cnt += 1;
	capture = 0;
	move::elements el{ move::decode(move) };

	auto sq1_64{ 1ULL << el.sq1 };
	auto sq2_64{ 1ULL << el.sq2 };

	assert(el.sq1 >= H1 && el.sq1 <= A8);
	assert(el.sq2 >= H1 && el.sq2 <= A8);
	assert(el.piece != NONE);
	assert(el.piece == piece_sq[el.sq1]);
	assert(el.victim != KINGS);

	// checking if castling rights have to be adjusted when the rook is engaged

	uint8 old_castling_right[4];
	for (auto i{ 0 }; i < 4; ++i)
		old_castling_right[i] = castling_right[i];

	rook_is_engaged(el.sq1, el.piece);
	rook_is_engaged(el.sq2, el.victim);

	// removing the eventually captured piece

	if (el.victim != NONE)
	{
		// capturing enpassant

		if (el.flag == ENPASSANT)
		{
			assert(ep_sq != 0ULL);

			auto victim{ bit::shift(ep_sq, push[xturn]) };
			assert(victim & pieces[PAWNS] & side[xturn]);

			pieces[PAWNS] &= ~victim;
			side[xturn] &= ~victim;

			auto sq{ bit::scan(victim) };
			assert(piece_sq[sq] == PAWNS);
			piece_sq[sq] = NONE;

			auto rand_key{ zobrist::rand_key[xturn * 64 + sq] };
			key ^= rand_key;
			pawn_key ^= rand_key;
		}

		// capturing normally
		
		else
		{
			assert(sq2_64 & side[xturn]);
			side[xturn] &= ~sq2_64;
			pieces[el.victim] &= ~sq2_64;
			key ^= zobrist::rand_key[(el.victim * 2 + xturn) * 64 + el.sq2];

			if (el.victim == PAWNS)
				pawn_key ^= zobrist::rand_key[xturn * 64 + el.sq2];
		}

		half_move_cnt = 0;
		capture = el.sq2;
	}

	// adjusting the half-move-clock & the pawn hash key with every pawn move

	if (el.piece == PAWNS)
	{
		half_move_cnt = 0;
		pawn_key ^= zobrist::rand_key[turn * 64 + el.sq1];
		pawn_key ^= zobrist::rand_key[turn * 64 + el.sq2];
	}

	// checking the validity of the enpassant square

	if (ep_sq)
	{
		auto ep_idx{ bit::scan(ep_sq) };
		if (pieces[PAWNS] & side[turn] & attack::king_map[ep_idx] & (bit::rank[R4] | bit::rank[R5]))
			key ^= zobrist::rand_key[zobrist::off.ep + square::file(ep_idx)];
		ep_sq = 0ULL;
	}

	// doublepushing pawn

	if (el.flag == DOUBLEPUSH)
	{
		assert(el.piece == PAWNS && el.victim == NONE);
		auto ep_idx{ (el.sq1 + el.sq2) / 2 };
		ep_sq = 1ULL << ep_idx;

		if (pieces[PAWNS] & side[xturn] & attack::king_map[ep_idx] & (bit::rank[R4] | bit::rank[R5]))
			key ^= zobrist::rand_key[zobrist::off.ep + square::file(ep_idx)];
	}

	// decoding castling

	if (move::is_castling(el.flag))
	{
		el.sq2 = king_target[turn][el.flag - SHORT];
		sq2_64 = 1ULL << el.sq2;
	}

	// rearranging the pieces

	pieces[el.piece] ^= sq1_64;
	pieces[el.piece] |= sq2_64;

	side[turn] ^= sq1_64;
	side[turn] |= sq2_64;

	piece_sq[el.sq1] = NONE;
	piece_sq[el.sq2] = el.piece;

	auto idx{ (el.piece * 2 + turn) * 64 };
	key ^= zobrist::rand_key[idx + el.sq1];
	key ^= zobrist::rand_key[idx + el.sq2];

	if (el.flag >= 10)
	{
		// including the rook move when castling

		if (el.flag == castling::SHORT)
			rook_castling(castling_right[turn], rook_target[turn][0]);
		else if (el.flag == castling::LONG)
			rook_castling(castling_right[turn + 2], rook_target[turn][1]);

		// promoting pawns

		else
		{
			auto promo_piece{ el.flag - 11 };

			assert(el.flag >= 12 && el.flag <= 15);
			assert(pieces[PAWNS] & sq2_64);
			assert(piece_sq[el.sq2] == PAWNS);

			pieces[PAWNS] ^= sq2_64;
			pieces[promo_piece] |= sq2_64;
			piece_sq[el.sq2] = promo_piece;

			key ^= zobrist::rand_key[turn * 64 + el.sq2];
			key ^= zobrist::rand_key[(promo_piece * 2 + turn) * 64 + el.sq2];
			pawn_key ^= zobrist::rand_key[turn * 64 + el.sq2];
		}
	}

	// adjusting the castling rights when the king moves

	if (sq2_64 & pieces[KINGS])
	{
		castling_right[turn] = PROHIBITED;
		castling_right[turn + 2] = PROHIBITED;

		king_sq[turn] = el.sq2;
	}

	// updating the hash key when castling rights change

	for (auto i{ 0 }; i < 4; ++i)
	{
		if (old_castling_right[i] != castling_right[i])
		{
			assert(castling_right[i] == PROHIBITED);
			key ^= zobrist::rand_key[zobrist::off.castling + i];
		}
	}

	// updating side to move

	xturn ^= 1;
	turn  ^= 1;
	key ^= zobrist::rand_key[zobrist::off.turn];
	side[BOTH] = side[WHITE] | side[BLACK];

	assert(turn == (xturn ^ 1));
	assert(side[BOTH] == (side[WHITE] ^ side[BLACK]));
	assert_exp(trans::to_key(*this) == key);
	assert_exp(pawn::to_key(*this)  == pawn_key);
}

void board::revert(board &prev_pos)
{
	*this = prev_pos;
}

void board::rook_is_engaged(int sq, int piece)
{
	// updating castling rights when rook moves

	if (piece == ROOKS)
	{
		for (auto &right : castling_right)
			if (right == sq)
			{
				right = PROHIBITED;
				break;
			}
	}
}

void board::rook_castling(int sq1, int sq2)
{
	// moving the rook to castle

	assert(sq1 <= A8 && sq1 >= H1);

	pieces[ROOKS] ^= (1ULL << sq1);
	pieces[ROOKS] |= (1ULL << sq2);
	
	if (piece_sq[sq1] == ROOKS)
	{
		piece_sq[sq1] = NONE;
		side[turn] ^= (1ULL << sq1);
	}
	else
		assert(piece_sq[sq1] == KINGS);

	side[turn] |= (1ULL << sq2);
	piece_sq[sq2] = ROOKS;

	key ^= zobrist::rand_key[(6 + turn) * 64 + sq1];
	key ^= zobrist::rand_key[(6 + turn) * 64 + sq2];
}

void board::null_move(uint64 &ep_copy, uint16 &capture_copy)
{
	// doing a "null move" i.e. not moving, used for null move pruning

	key ^= zobrist::rand_key[zobrist::off.turn];
	capture_copy = capture;

	if (ep_sq)
	{
		auto ep_idx{ bit::scan(ep_sq) };
		if (pieces[PAWNS] & side[turn] & attack::king_map[ep_idx] & (bit::rank[R4] | bit::rank[R5]))
			key ^= zobrist::rand_key[zobrist::off.ep + square::file(ep_idx)];
		ep_copy = ep_sq;
		ep_sq = 0;
	}

	half_move_cnt += 1;
	move_cnt += 1;
	turn  ^= 1;
	xturn ^= 1;

	assert_exp(trans::to_key(*this) == key);
}

void board::revert_null_move(uint64 &ep_copy, uint16 &capture_copy)
{
	// undoing the "null move"

	key ^= zobrist::rand_key[zobrist::off.turn];
	capture = capture_copy;

	if (ep_copy)
	{
		auto ep_idx{ bit::scan(ep_copy) };
		if (pieces[PAWNS] & side[xturn] & attack::king_map[ep_idx] & (bit::rank[R4] | bit::rank[R5]))
			key ^= zobrist::rand_key[zobrist::off.ep + square::file(ep_idx)];
		ep_sq = ep_copy;
	}

	half_move_cnt -= 1;
	move_cnt -= 1;
	turn  ^= 1;
	xturn ^= 1;

	assert_exp(trans::to_key(*this) == key);
}

void board::clear()
{
	// resetting the board to blankness

	std::fill(pieces, pieces + 6, 0ULL);
	std::fill(side, side + 3, 0ULL);
	std::fill(piece_sq, piece_sq + 64, NONE);
	std::fill(castling_right, castling_right + 4, PROHIBITED);

	move_cnt = 0;
	half_move_cnt = 0;
	capture = 0;
	ep_sq = 0ULL;
	turn  = WHITE;
	xturn = BLACK;
}

uint8 board::get_rook_sq(int col, int dir) const
{
	// getting the position of the rook if castling is allowed (used for Chess960)

	auto sq{ king_sq[col] };
	for (; piece_sq[sq] != ROOKS; sq += dir);
	return sq;
}

void board::parse_fen(std::string fen)
{
	// conveying the FEN-string to the board

	int sq{ H1 };
	std::istringstream stream(fen);
	std::string token{ };

	clear();
	stream >> token;

	// getting all pieces in place

	for (; token.size() != 0; token.pop_back())
	{
		assert(sq >= H1 && sq <= A8);

		auto focus{ token.back() };
		if (focus == '/')
		{
			token.pop_back();
			focus = token.back();
		}

		if (isdigit(focus))
			sq += focus - '0';
		else
		{
			int col{ islower(focus) ? BLACK : WHITE };
			auto letter{ static_cast<char>(toupper(focus)) };

			for (int p{ PAWNS }; p <= KINGS; ++p)
			{
				if (letter != "PNBRQK"[p])
					continue;

				pieces[p] |= 1ULL << sq;
				side[col] |= 1ULL << sq;
				piece_sq[sq] = p;
				break;
			}
			sq += 1;
		}
	}

	side[BOTH] = side[WHITE] | side[BLACK];
	assert(side[BOTH] == (side[WHITE] ^ side[BLACK]));

	// finding the king squares

	king_sq[WHITE] = static_cast<uint8>(bit::scan(pieces[KINGS] & side[WHITE]));
	king_sq[BLACK] = static_cast<uint8>(bit::scan(pieces[KINGS] & side[BLACK]));

	// setting the side to move

	stream >> token;
	assert(token == "w" || token == "b");

	turn = { token == "w" ? WHITE : BLACK };
	xturn = turn ^ 1;

	// setting castling rights

	stream >> token;

	if (token != "-")
	{
		for (; token.size() != 0; token.pop_back())
		{
			int col{ islower(token.back()) ? BLACK : WHITE };
			auto letter{ toupper(token.back()) };

			if (letter == 'K')
			{
				castling_right[col] = get_rook_sq(col, -1);
			}
			else if (letter == 'Q')
			{
				castling_right[col + 2] = get_rook_sq(col, 1);
			}
			else if (letter >= 'A' && letter <= 'H')
			{
				auto file_rook{ 'H' - letter };
				auto idx{ file_rook > square::file(king_sq[col]) ? col + 2 : col };

				castling_right[idx] = file_rook + col * 56;
			}
		}
	}

	// setting possible enpassant square

	stream >> token;
	if (token != "-")
		ep_sq = 1ULL << square::index(token);

	// creating hash keys & checking if the fen is complete

	key = trans::to_key(*this);
	pawn_key = pawn::to_key(*this);
	if (!(stream >> token))
		return;

	// setting half-move & move count

	half_move_cnt = stoi(token);
	stream >> move_cnt;
	move_cnt = move_cnt * 2 - 1 - turn;
}

bool board::check() const
{
	// returning true if the moving side is in check

	return !attack::check(*this, turn, pieces[KINGS] & side[turn]);
}

bool board::lone_king() const
{
	// returning true if there are only pawns left on the board

	return (pieces[KINGS] | pieces[PAWNS]) == side[BOTH];
}

bool board::recapture(uint32 move) const
{
	// returning true if the move is a recapture

	return move::sq2(move) == capture;
}

bool board::pseudolegal(uint32 move) const
{
	// asserting the correct match between the board and <move>

	if (move == NO_MOVE)
		return false;

	move::elements el{ move::decode(move) };
	auto sq1_64{ 1ULL << el.sq1 };
	auto sq2_64{ 1ULL << el.sq2 };

	// assessing the side to move

	if (el.turn != turn)
		return false;

	// assessing start square sq1

	if (!(sq1_64 & (pieces[el.piece] & side[el.turn])))
		return false;

	// assessing end square sq2

	if (move::is_castling(el.flag))
	{
		if (!(sq2_64 & (pieces[ROOKS] & side[el.turn])))
			return false;
	}
	else if (el.victim == NONE || el.flag == ENPASSANT)
	{
		if (piece_sq[el.sq2] != NONE)
			return false;
	}
	else
	{
		assert(el.victim != NONE);
		if (!(sq2_64 & (pieces[el.victim] & side[el.turn ^ 1])))
			return false;
	}

	// assessing the path between sq1 & sq2

	switch (el.piece)
	{
	case PAWNS:
		if (el.flag == DOUBLEPUSH)
			return piece_sq[(el.sq1 + el.sq2) / 2] == NONE;
		else if (el.flag == ENPASSANT)
			return ep_sq == sq2_64;
		else
			return true;

	case KNIGHTS:
		return true;

	case BISHOPS:
		return sq2_64 & attack::by_slider<BISHOP>(el.sq1, side[BOTH]);

	case ROOKS:
		return sq2_64 & attack::by_slider<ROOK>(el.sq1, side[BOTH]);

	case QUEENS:
		return sq2_64 & attack::by_slider<QUEEN>(el.sq1, side[BOTH]);

	case KINGS:
		if (el.flag == NONE)
		{
			return true;
		}
		else
		{
			// assessing the messy right to castle

			assert(move::is_castling(el.flag));

			if (castling_right[turn + 2 * (el.flag - SHORT)] == PROHIBITED)
				return false;

			switch (el.flag)
			{
			case castling::SHORT:
			{
				auto king_target_64{ 1ULL << king_target[turn][0] };
				auto rook_64{ 1ULL << castling_right[turn] };

				auto bound_max{ 1ULL << std::max(static_cast<uint8>(rook_target[turn][0] + 1), king_sq[turn]) };
				auto bound_min{ std::min(king_target_64, rook_64) };
				auto occupancy{ side[BOTH] ^ (1ULL << king_sq[turn] | rook_64) };

				if (((bound_max - 1) & ~(bound_min - 1)) & occupancy)
					return false;

				auto no_check_zone{ ((1ULL << (king_sq[turn] + 1)) - 1) & ~(king_target_64 - 1) };
				board pos_copy{ *this };
				pos_copy.side[BOTH] ^= rook_64;

				if (attack::check(pos_copy, turn, no_check_zone) != no_check_zone)
					return false;

				return true;
			}
			case castling::LONG:
			{
				auto rook_64{ 1ULL << castling_right[turn + 2] };

				auto bound_max{ 1ULL << std::max(static_cast<uint8>(king_target[turn][1] + 1), castling_right[turn + 2]) };
				auto bound_min{ 1ULL << std::min(rook_target[turn][1], king_sq[turn]) };
				auto occupancy{ side[BOTH] & ~((1ULL << king_sq[turn]) | rook_64) };

				if (((bound_max - 1) ^ (bound_min - 1)) & occupancy)
					return false;

				auto no_check_zone{ (1ULL << king_sq[turn]) | (1ULL << king_target[turn][1]) | (((1ULL << king_target[turn][1]) - 1) & ~((1ULL << king_sq[turn]) - 1)) };
				board pos_copy{ *this };
				pos_copy.side[BOTH] ^= rook_64;

				if (attack::check(pos_copy, turn, no_check_zone) != no_check_zone)
					return false;

				return true;
			}
			default:
				assert(false);
				return false;
			}
		}

	default:
		assert(false);
		return false;
	}
}