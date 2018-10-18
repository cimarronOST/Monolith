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


#include <sstream>

#include "utilities.h"
#include "move.h"
#include "pawn.h"
#include "trans.h"
#include "attack.h"
#include "zobrist.h"
#include "bit.h"
#include "position.h"

void board::new_move(uint32 move)
{
	// updating the board with a new move

	move_count += 1;
	half_count += 1;
	capture = 0;
	move::elements el{ move::decode(move) };

	auto sq1_bit{ 1ULL << el.sq1 };
	auto sq2_bit{ 1ULL << el.sq2 };

	assert(el.sq1 >= H1 && el.sq1 <= A8);
	assert(el.sq2 >= H1 && el.sq2 <= A8);
	assert(el.piece != NONE);
	assert(el.piece == piece[el.sq1]);
	assert(el.victim != KINGS);

	// checking if castling rights have to be adjusted when the rook is engaged

	uint8 old_castling_right[4]{};
	for (int i{}; i < 4; ++i)
		old_castling_right[i] = castling_right[i];

	rook_is_engaged(el.sq1, el.piece);
	rook_is_engaged(el.sq2, el.victim);

	// removing the eventually captured piece

	if (el.victim != NONE)
	{
		// capturing en-passant

		if (el.flag == ENPASSANT)
		{
			assert(ep_rear != 0ULL);

			auto victim{ bit::shift(ep_rear, shift::push[xturn]) };
			assert(victim & pieces[PAWNS] & side[xturn]);

			pieces[PAWNS] &= ~victim;
			side[xturn]   &= ~victim;

			auto sq{ bit::scan(victim) };
			assert(piece[sq] == PAWNS);
			piece[sq] = NONE;

			auto rand_key{ zobrist::rand_key[(PAWNS * 2 + xturn) * 64 + sq] };
			key      ^= rand_key;
			pawn_key ^= rand_key;
		}

		// capturing normally
		
		else
		{
			assert(sq2_bit & side[xturn]);
			side[xturn]       &= ~sq2_bit;
			pieces[el.victim] &= ~sq2_bit;
			key ^= zobrist::rand_key[(el.victim * 2 + xturn) * 64 + el.sq2];

			if (el.victim == PAWNS)
				pawn_key ^= zobrist::rand_key[xturn * 64 + el.sq2];
		}

		half_count = 0;
		capture = el.sq2;
	}

	// adjusting the half-move-clock & the pawn hash key with every pawn move

	if (el.piece == PAWNS)
	{
		half_count = 0;
		pawn_key ^= zobrist::rand_key[turn * 64 + el.sq1];
		pawn_key ^= zobrist::rand_key[turn * 64 + el.sq2];
	}

	// checking the validity of the en-passant square

	if (ep_rear)
	{
		auto ep_index{ bit::scan(ep_rear) };
		if (pieces[PAWNS] & side[turn] & attack::king_map[ep_index] & (bit::rank[R4] | bit::rank[R5]))
			key ^= zobrist::rand_key[zobrist::off.ep + index::file(ep_index)];
		ep_rear = 0ULL;
	}

	// double-pushing pawn

	if (el.flag == DOUBLEPUSH)
	{
		assert(el.piece == PAWNS && el.victim == NONE);
		auto ep_index{ (el.sq1 + el.sq2) / 2 };
		ep_rear = 1ULL << ep_index;

		if (pieces[PAWNS] & side[xturn] & attack::king_map[ep_index] & (bit::rank[R4] | bit::rank[R5]))
			key ^= zobrist::rand_key[zobrist::off.ep + index::file(ep_index)];
	}

	// decoding castling

	if (move::castling(el.flag))
	{
		el.sq2 = square::king_target[turn][move::castle_side(el.flag)];
		sq2_bit = 1ULL << el.sq2;
	}

	// rearranging the pieces

	pieces[el.piece] ^= sq1_bit;
	pieces[el.piece] |= sq2_bit;

	side[turn] ^= sq1_bit;
	side[turn] |= sq2_bit;

	piece[el.sq1] = NONE;
	piece[el.sq2] = el.piece;

	auto offset{ (el.piece * 2 + turn) * 64 };
	key ^= zobrist::rand_key[offset + el.sq1];
	key ^= zobrist::rand_key[offset + el.sq2];

	if (el.flag >= 10)
	{
		// including the rook move when castling

		if (el.flag == CASTLE_SHORT)
			rook_castling(castling_right[turn],     square::rook_target[turn][move::castle_side(CASTLE_SHORT)]);
		else if (el.flag == CASTLE_LONG)
			rook_castling(castling_right[turn + 2], square::rook_target[turn][move::castle_side(CASTLE_LONG)]);

		// promoting pawns

		else
		{
			auto promo_piece{ move::promo_piece(el.flag) };

			assert(pieces[PAWNS] & sq2_bit);
			assert(piece[el.sq2] == PAWNS);

			pieces[PAWNS]       ^= sq2_bit;
			pieces[promo_piece] |= sq2_bit;
			piece[el.sq2] = promo_piece;

			key ^= zobrist::rand_key[(PAWNS       * 2 + turn) * 64 + el.sq2];
			key ^= zobrist::rand_key[(promo_piece * 2 + turn) * 64 + el.sq2];
			pawn_key ^= zobrist::rand_key[turn * 64 + el.sq2];
		}
	}

	// adjusting the castling rights when the king moves

	if (sq2_bit & pieces[KINGS])
	{
		castling_right[turn]     = PROHIBITED;
		castling_right[turn + 2] = PROHIBITED;

		sq_king[turn] = el.sq2;
	}

	// updating the hash key when castling rights change

	for (int i{}; i < 4; ++i)
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
	key   ^= zobrist::rand_key[zobrist::off.turn];
	side[BOTH] = side[WHITE] | side[BLACK];

	assert(turn == (xturn ^ 1));
	assert(side[BOTH] == (side[WHITE] ^ side[BLACK]));
	assert_exp(trans::to_key(*this) == key);
	assert_exp( pawn::to_key(*this) == pawn_key);
}

void board::revert(board &prev_pos)
{
	// reverting the position to <prev_pos>

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
	// moving the rook to its position when castling

	assert(sq1 <= A8 && sq1 >= H1);

	pieces[ROOKS] ^= (1ULL << sq1);
	pieces[ROOKS] |= (1ULL << sq2);
	
	if (piece[sq1] == ROOKS)
	{
		piece[sq1]  = NONE;
		side[turn] ^= (1ULL << sq1);
	}
	else
		assert(piece[sq1] == KINGS);

	side[turn] |= (1ULL << sq2);
	piece[sq2]  = ROOKS;

	key ^= zobrist::rand_key[(ROOKS * 2 + turn) * 64 + sq1];
	key ^= zobrist::rand_key[(ROOKS * 2 + turn) * 64 + sq2];
}

void board::null_move(uint64 &ep_copy, uint16 &capture_copy)
{
	// doing a "null move" by not moving but pretending to have moved, used for null move pruning
	// special care for en-passant has to be taken

	key ^= zobrist::rand_key[zobrist::off.turn];
	capture_copy = capture;

	if (ep_rear)
	{
		auto ep_index{ bit::scan(ep_rear) };
		if (pieces[PAWNS] & side[turn] & attack::king_map[ep_index] & (bit::rank[R4] | bit::rank[R5]))
			key ^= zobrist::rand_key[zobrist::off.ep + index::file(ep_index)];
		ep_copy = ep_rear;
		ep_rear = 0;
	}

	half_count += 1;
	move_count += 1;
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
		auto ep_index{ bit::scan(ep_copy) };
		if (pieces[PAWNS] & side[xturn] & attack::king_map[ep_index] & (bit::rank[R4] | bit::rank[R5]))
			key ^= zobrist::rand_key[zobrist::off.ep + index::file(ep_index)];
		ep_rear = ep_copy;
	}

	half_count -= 1;
	move_count -= 1;
	turn  ^= 1;
	xturn ^= 1;

	assert_exp(trans::to_key(*this) == key);
}

void board::clear()
{
	// resetting the board to blankness

	std::fill(pieces, pieces +  6, 0ULL);
	std::fill(side,   side   +  3, 0ULL);
	std::fill(piece,  piece  + 64, NONE);
	std::fill(castling_right, castling_right + 4, PROHIBITED);

	move_count = 0;
	half_count = 0;
	capture = 0;
	ep_rear = 0ULL;
	turn  = WHITE;
	xturn = BLACK;
}

uint8 board::sq_rook(int col, int dir) const
{
	// getting the position of the rook if castling is allowed (used for Chess960)

	auto sq{ sq_king[col] };
	for ( ; piece[sq] != ROOKS; sq += dir);
	return sq;
}

void board::parse_fen(std::string fen)
{
	// conveying the FEN-string to the board

	int sq{ H1 };
	std::istringstream stream(fen);
	std::string token{};

	clear();

	// getting all pieces in place

	for (stream >> token; !token.empty(); token.pop_back())
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
			int color{ islower(focus) ? BLACK : WHITE };
			auto letter{ static_cast<char>(toupper(focus)) };

			for (int p{ PAWNS }; p <= KINGS; ++p)
			{
				if (letter != "PNBRQK"[p])
					continue;

				pieces[p]   |= 1ULL << sq;
				side[color] |= 1ULL << sq;
				piece[sq] = p;
				break;
			}
			sq += 1;
		}
	}

	side[BOTH] = side[WHITE] | side[BLACK];
	assert(side[BOTH] == (side[WHITE] ^ side[BLACK]));

	// finding the king squares

	sq_king[WHITE] = static_cast<uint8>(bit::scan(pieces[KINGS] & side[WHITE]));
	sq_king[BLACK] = static_cast<uint8>(bit::scan(pieces[KINGS] & side[BLACK]));

	// setting the side to move

	stream >> token;
	assert(token == "w" || token == "b");

	turn  = { token == "w" ? WHITE : BLACK };
	xturn = turn ^ 1;

	// setting castling rights

	stream >> token;
	if (token != "-")
	{
		for ( ; !token.empty(); token.pop_back())
		{
			int color{ islower(token.back()) ? BLACK : WHITE };
			auto letter{ toupper(token.back()) };

			if (letter == 'K')
			{
				castling_right[color] = sq_rook(color, -1);
			}
			else if (letter == 'Q')
			{
				castling_right[color + 2] = sq_rook(color, 1);
			}
			else if (letter >= 'A' && letter <= 'H')
			{
				auto file_rook{ 'H' - letter };
				auto index{ file_rook > index::file(sq_king[color]) ? color + 2 : color };

				castling_right[index] = file_rook + color * H8;
			}
		}
	}

	// setting possible en-passant square

	stream >> token;
	if (token != "-")
		ep_rear = 1ULL << square::index(token);

	// creating hash keys & checking if the fen is complete

	key      = trans::to_key(*this);
	pawn_key =  pawn::to_key(*this);
	if (!(stream >> token))
		return;

	// setting half-move- & move-count

	half_count = stoi(token);
	stream >> move_count;
	move_count = move_count * 2 - 1 - turn;
}

bool board::check() const
{
	// returning true if the moving side is in check

	return !attack::check(*this, turn, pieces[KINGS] & side[turn]);
}

bool board::gives_check(uint32 move)
{
	// determining if <move> gives check (assuming it is a legal move)

	assert(move != MOVE_NONE);
	assert(move::turn(move) == turn);
	assert(piece[move::sq1(move)] != NONE);

	auto sq1 { move::sq1(move) };
	auto sq2 { move::sq2(move) };
	auto flag{ move::flag(move) };
	auto occ{ side[BOTH] };

	if (move::castling(move))
	{
		// considering only the rook

		occ ^= 1ULL << sq1;
		sq1  = sq2;
		sq2  = square::rook_target[turn][move::castle_side(flag)];
	}

	auto p{ piece[sq1] };
	auto sq1_bit{ 1ULL << sq1 };
	auto sq2_bit{ 1ULL << sq2 };
	
	occ ^= sq1_bit;
	occ |= sq2_bit;

	// direct check

	if (move::promo(move))
		p = move::promo_piece(flag);

	if (attack::by_piece(p, sq2, turn, occ) & pieces[KINGS] & side[xturn])
		return true;

	// discovered check

	bool check{};
	if (flag == ENPASSANT)
	{
		sq1_bit |= bit::shift(ep_rear, shift::push[xturn]);
		occ    &= ~sq1_bit;
	}

	if (attack::slide_map[ROOK][sq_king[xturn]] & sq1_bit)
		check = attack::by_slider<ROOK>(sq_king[xturn], occ) & (pieces[ROOKS] | pieces[QUEENS]) & occ & side[turn];

	if (attack::slide_map[BISHOP][sq_king[xturn]] & sq1_bit)
		check = check || (attack::by_slider<BISHOP>(sq_king[xturn], occ) & (pieces[BISHOPS] | pieces[QUEENS]) & occ & side[turn]);

	return check;
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

bool board::repetition(uint64 hash[], int offset) const
{
	// marking every one-fold-repetition as a draw
	// hash[] has a big safety margin for GUIs like Arena that don't always recognize a 50-move-draw immediately

	assert(offset <= lim::depth + 256);
	assert(hash[offset] == key);

	for (int i{ 4 }; i <= half_count && i <= offset; i += 2)
	{
		if (hash[offset - i] == hash[offset])
			return true;
	}
	return false;
}

bool board::draw(uint64 hash[], int offset) const
{
	// detecting draw-by-50-move-rule & draw-by-repetition
	// draw-by-insufficient-mating-material is handled by the static evaluation

	return repetition(hash, offset) || half_count >= 100;
}

bool board::pseudolegal(uint32 move) const
{
	// asserting the correct match between the board and a move
	// a correct move-format is assumed

	if (move == MOVE_NONE)
		return false;

	move::elements el{ move::decode(move) };
	auto sq1_bit{ 1ULL << el.sq1 };
	auto sq2_bit{ 1ULL << el.sq2 };

	// assessing the side to move

	if (el.turn != turn)
		return false;

	// assessing start square sq1

	if (!(sq1_bit & (pieces[el.piece] & side[el.turn])))
		return false;

	// assessing end square sq2

	if (move::castling(el.flag))
	{
		if (!(sq2_bit & (pieces[ROOKS] & side[el.turn])))
			return false;
	}
	else if (el.victim == NONE || el.flag == ENPASSANT)
	{
		if (piece[el.sq2] != NONE)
			return false;
	}
	else
	{
		assert(el.victim != NONE);
		if (!(sq2_bit & (pieces[el.victim] & side[el.turn ^ 1])))
			return false;
	}

	// checking whether the path between sq1 & sq2 is free

	switch (el.piece)
	{
	case PAWNS:
		if (el.flag == DOUBLEPUSH)
			return piece[(el.sq1 + el.sq2) / 2] == NONE;
		else if (el.flag == ENPASSANT)
			return ep_rear == sq2_bit;
		else
			return true;

	case KNIGHTS:
		return true;

	case BISHOPS:
		return sq2_bit & attack::by_slider<BISHOP>(el.sq1, side[BOTH]);

	case ROOKS:
		return sq2_bit & attack::by_slider<ROOK>(el.sq1, side[BOTH]);

	case QUEENS:
		return sq2_bit & attack::by_slider<QUEEN>(el.sq1, side[BOTH]);

	case KINGS:
		if (el.flag == NONE)
		{
			return true;
		}
		else
		{
			// assessing the messy right to castle

			assert(move::castling(el.flag));

			if (castling_right[turn + 2 * move::castle_side(el.flag)] == PROHIBITED)
				return false;

			switch (el.flag)
			{
			case CASTLE_SHORT:
			{
				auto king_target_bit{ 1ULL << square::king_target[turn][0] };
				auto rook_bit{ 1ULL << castling_right[turn] };

				auto bound_max{ 1ULL
					<< std::max(static_cast<uint8>(square::rook_target[turn][move::castle_side(CASTLE_SHORT)] + 1), sq_king[turn]) };
				auto bound_min{ std::min(king_target_bit, rook_bit) };
				auto occupancy{ side[BOTH] ^ (1ULL << sq_king[turn] | rook_bit) };

				if (((bound_max - 1) & ~(bound_min - 1)) & occupancy)
					return false;

				auto no_check_zone{ ((1ULL << (sq_king[turn] + 1)) - 1) & ~(king_target_bit - 1) };
				board pos_copy{ *this };
				pos_copy.side[BOTH] ^= rook_bit;

				if (attack::check(pos_copy, turn, no_check_zone) != no_check_zone)
					return false;

				return true;
			}
			case CASTLE_LONG:
			{
				auto rook_bit{ 1ULL << castling_right[turn + 2] };

				auto bound_max{ 1ULL << std::max(static_cast<uint8>(square::king_target[turn][1] + 1), castling_right[turn + 2]) };
				auto bound_min{ 1ULL << std::min(square::rook_target[turn][move::castle_side(CASTLE_LONG)], sq_king[turn]) };
				auto occupancy{ side[BOTH] & ~((1ULL << sq_king[turn]) | rook_bit) };

				if (((bound_max - 1) ^ (bound_min - 1)) & occupancy)
					return false;

				auto no_check_zone{ (1ULL << sq_king[turn]) | (1ULL << square::king_target[turn][1])
					| (((1ULL << square::king_target[turn][1]) - 1) & ~((1ULL << sq_king[turn]) - 1)) };
				board pos_copy{ *this };
				pos_copy.side[BOTH] ^= rook_bit;

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

bool board::legal() const
{
	// checking if the position is legal
	// can be used to check if a move is legal by calling legal() after the move was made

	return attack::check(*this, xturn, pieces[KINGS] & side[xturn]);
}