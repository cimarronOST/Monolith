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


#include "movegen.h"
#include "bitboard.h"
#include "convert.h"

namespace
{
	const uint8 piece_char[]{ 'R', 'N', 'B', 'Q', 'K' };
}

uint64 conv::to_bb(const string sq)
{
	return 1ULL << to_int(sq);
}
int conv::to_int(const string sq)
{
	assert(sq.size() == 2U);
	assert( isdigit(unsigned(sq.back())));
	assert(!isdigit(unsigned(sq.front())));

	int shift{ 'h' - sq.front() };
	shift += (sq.back() - '1') * 8;

	assert(shift >= 0);
	assert(shift <= 63);

	return shift;
}

string conv::to_str(const uint64 &sq)
{
	bb::bitscan(sq);
	return to_str(static_cast<int>(bb::lsb()));
}
string conv::to_str(const int sq)
{
	string str;
	str += 'h' - static_cast<char>(sq & 7);
	str += '1' + static_cast<char>(sq >> 3);

	return str;
}

uint16 conv::san_to_move(pos &board, string move)
{
	uint8 flag{ 0 };
	uint16 sq1{ 0 }, sq2{ 0 };

	// castling short

	if (move == "O-O")
	{
		const uint16 sqs[]{ E1, G1, E8, G8 };

		sq1 = sqs[board.turn * 2];
		sq2 = sqs[board.turn * 2 + 1];
		flag = castl_e::SHORT_WHITE + board.turn * 2;
	}

	// castling long

	else if (move == "O-O-O")
	{
		const uint16 sqs[]{ E1, C1, E8, C8 };

		sq1 = sqs[board.turn * 2];
		sq2 = sqs[board.turn * 2 + 1];
		flag = castl_e::LONG_WHITE + board.turn * 2;
	}
	else
	{
		if (move.back() == '+')
			move.pop_back();

		// promotion

		char promo{ move.back() };

		for (int i{ 0 }; i < 4; ++i)
			if (piece_char[i] == promo)
			{
				promo = islower(piece_char[i]);
				move.pop_back();
				break;
			}

		// creating square 1

		if (move.size() >= 2
			&&  isdigit(static_cast<unsigned>(move.back()))
			&& !isdigit(static_cast<unsigned>(move[move.size() - 2])))
		{
			sq2 = static_cast<uint16>(to_int(move.substr(move.size() - 2, 2)));
			move.erase(move.size() - 2, move.size());
		}

		// in case of a pawn, piece = file of the pawn

		char piece{ 0 };
		if (!move.empty())
		{
			piece = move.front();
			move.erase(0, 1);
		}

		// square specifies the file or rank of the piece

		char square{ 0 };
		if (move != "x" && !move.empty())
			square = move.front();

		movegen gen(board, ALL);

		// creating square 1

		int possible[8]{ 0 };
		int count{ 0 };
		for (int idx{ 0 }; idx < gen.move_cnt; ++idx)
		{
			if (to_sq2(gen.list[idx]) == sq2)
				possible[count++] = idx;
		}

		// no legal move found

		if (count == 0)
			return 0;

		// one possible move
		else if (count == 1)
			sq1 = to_sq1(gen.list[possible[0]]);

		// more possible moves

		else
		{
			uint64 occ{ 0 };

			// piece move

			for (int p{ ROOKS }; p <= KINGS; ++p)
				if (piece_char[p-1] == piece)
				{
					occ = board.pieces[p];
					break;
				}

			// pawn move

			if (!occ && piece)
			{
				occ = board.pieces[PAWNS];
				occ &= file[H] << ('h' - piece);
			}

			// piece rank/file

			if (square != 0)
			{
				int sq{ square - '1' };
				if (sq >= 0 && sq < 8)
					occ &= rank[R1] << (sq * 8);
				else
				{
					sq = 'h' - square;
					assert(sq >= 0 && sq < 8);
					occ &= file[H] << sq;
				}
			}

			// finding the right move

			for (int idx{ 0 }; idx < count; ++idx)
				if ((1ULL << to_sq1(gen.list[possible[idx]])) & occ)
				{
					sq1 = to_sq1(gen.list[possible[idx]]);
					break;
				}

			assert(sq1 != 0);
		}

		uint64 sq1_64{ 1ULL << sq1 };
		uint64 sq2_64{ 1ULL << sq2 };
		flag = to_flag(promo, board, sq1_64, sq2_64);
	}

	return encode(sq1, sq2, flag);
}
string conv::bit_to_san(pos &board, const uint64 &sq1_64, const uint64 &sq2_64, uint8 flag)
{
	// works only if engine::new_move() has not yet been called

	string move;
	movegen gen(board, ALL);
	assert(gen.move_cnt != 0);

	bb::bitscan(sq1_64);
	int sq1{ static_cast<int>(bb::lsb()) };
	bb::bitscan(sq2_64);
	int sq2{ static_cast<int>(bb::lsb()) };

	// castling short

	if (flag == castl_e::SHORT_WHITE || flag == castl_e::SHORT_BLACK)
	{
		assert((sq1 == E1 && sq2 == G1) || (sq1 == E8 && sq2 == G8));
		move = "O-O";
	}

	// castling long

	else if (flag == castl_e::LONG_WHITE || flag == castl_e::LONG_BLACK)
	{
		assert((sq1 == E1 && sq2 == C1) || (sq1 == E8 && sq2 == C8));
		move = "O-O-O";
	}
	else
	{
		uint64 piece{ 0 };
		string sq1_str{ to_str(sq1_64) };
		auto pc{ board.piece_sq[sq1] };

		// adding piece or pawn-file

		if (pc != PAWNS)
		{
			move += piece_char[pc - 1];
			piece = board.pieces[pc];
		}
		else if (sq2_64 & board.side[BOTH])
		{
			assert(sq1_64 & board.pieces[PAWNS]);
			move = sq1_str.front();
		}

		// collecting all legal moves to square 2

		uint64 possible{ 0 };
		for (int cnt{ 0 }; cnt < gen.move_cnt; ++cnt)
		{
			if (1ULL << to_sq2(gen.list[cnt]) == sq2_64 && piece & (1ULL << to_sq1(gen.list[cnt])))
				possible |= 1ULL << to_sq1(gen.list[cnt]);
		}

		// specifying which move is the right one

		if (possible & (possible - 1))
		{
			auto rank_idx{ sq1_str.back() - '1' };
			auto file_idx{ 'h' - sq1_str.front() };

			assert(1ULL << (file_idx + 8 * rank_idx) == sq1_64);

			if (bb::popcnt(possible & rank[rank_idx]) >= 2)
				move += sq1_str.front();
			else if (bb::popcnt(possible & file[file_idx]) >= 2)
				move += sq1_str.back();
			else
				move += sq1_str.front();
		}

		// adding square 2

		if (sq2_64 & board.side[BOTH])
			move += "x";
		move += to_str(sq2_64);

		// adding promo

		if(to_promo(flag) != " ")
			move += to_promo(flag).front();
	}

	// execute the move to see if there is a check

	uint16 move_temp{ encode(sq1, sq2, flag) };

	pos save(board);
	assert(gen.in_list(move_temp));

	board.new_move(move_temp);

	uint64 enemy_king{ board.side[board.turn ^ 1] & board.pieces[KINGS] };
	if (!gen.check(board, board.turn ^ 1, enemy_king))
		move += "+";

	board = save;
	return move;
}

string conv::to_promo(uint8 flag)
{
	assert(flag <= 15);
	string promo{ " " };

	if (flag >= 12)
	{
		if (flag == PROMO_ROOK)
			promo = "r ";
		else if (flag == PROMO_KNIGHT)
			promo = "n ";
		else if (flag == PROMO_BISHOP)
			promo = "b ";
		else if (flag == PROMO_QUEEN)
			promo = "q ";
		else
			assert(false);
	}
	return promo;
}
uint8 conv::to_flag(char promo, const pos &board, const uint64 &sq1_64, const uint64 &sq2_64)
{
	assert(sq1_64 != 0 && sq2_64 != 0);

	bb::bitscan(sq1_64);
	int sq1{ static_cast<int>(bb::lsb()) };
	bb::bitscan(sq2_64);
	int sq2{ static_cast<int>(bb::lsb()) };

	// captured piece

	auto flag{ board.piece_sq[sq2] };
	auto piece{ board.piece_sq[sq1] };

	if (piece == PAWNS)
	{
		// enpassant

		if ((~board.side[BOTH] & sq2_64) && abs(sq1 - sq2) % 8 != 0)
			flag = ENPASSANT;

		// promotion

		if (promo == 'q') flag = PROMO_QUEEN;
		else if (promo == 'r') flag = PROMO_ROOK;
		else if (promo == 'n') flag = PROMO_KNIGHT;
		else if (promo == 'b') flag = PROMO_BISHOP;
	}

	// castling

	else if (piece == KINGS)
	{
		if (sq1 == E1 && sq2 == G1) flag = castl_e::SHORT_WHITE;
		else if (sq1 == E1 && sq2 == C1) flag = castl_e::LONG_WHITE;
		else if (sq1 == E8 && sq2 == G8) flag = castl_e::SHORT_BLACK;
		else if (sq1 == E8 && sq2 == C8) flag = castl_e::LONG_BLACK;
	}
	else
		assert(piece > PAWNS && piece < KINGS);

	return flag;
}
