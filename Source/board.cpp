/*
  Monolith 2 Copyright (C) 2017-2020 Jonas Mayr
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

#include "attack.h"
#include "zobrist.h"
#include "bit.h"
#include "board.h"

void board::parse_fen(const std::string& fen_string)
{
	// conveying a FEN-string into the internal board representation
	// a correct FEN-string is assumed

	reset();
	square sq{ h1 };
	std::string token{};
	std::istringstream fen{ fen_string };

	// starting with the piece placement

	for (fen >> token; !token.empty(); token.pop_back())
	{
		char ch{ token.back() };
		if (ch == '/')
		{
			token.pop_back();
			ch = token.back();
		}

		if (isdigit(ch))
			sq += ch - '0';
		else
		{
			color cl{ islower(ch) ? black : white };
			char  ch_upper{ char(toupper(ch)) };

			for (piece pc : { pawn, knight, bishop, rook, queen, king })
			{
				if (ch_upper != "PNBRQK"[pc])
					continue;

				pieces[pc] |= bit::set(sq);
				side[cl] |= bit::set(sq);
				piece_on[sq] = pc;
				break;
			}
			sq += 1;
		}
	}
	verify(sq == a8 + 1);
	verify(!(side[white] & side[black]));

	// determining squares of the kings

	sq_king[white] = bit::scan(pieces[king] & side[white]);
	sq_king[black] = bit::scan(pieces[king] & side[black]);
	verify(type::sq(sq_king[white]) && type::sq(sq_king[black]));

	// determining the side to move

	side[both] = side[white] | side[black];
	fen >> token;
	verify(token == "w" || token == "b");
	cl = token == "w" ? white : black;
	cl_x = cl ^ 1;

	// setting castling rights

	fen >> token;
	if (token != "-")
	{
		for (; !token.empty(); token.pop_back())
		{
			color cl{ islower(token.back()) ? black : white };
			char  ch{ char(toupper(token.back())) };

			if (ch == 'K')
			{
				castle_right[cl][castle_east] = castling_rook(cl, direction::east);
			}
			else if (ch == 'Q')
			{
				castle_right[cl][castle_west] = castling_rook(cl, direction::west);
			}
			else if (ch >= 'A' && ch <= 'H')
			{
				file f_rook{ file('H' - ch) };
				file f_king{ type::fl_of(sq_king[cl]) };
				castle_right[cl][f_rook < f_king ? castle_east : castle_west] = square(f_rook) + (cl == white ? h1 : h8);
			}
		}
	}

	// saving the rear square of a double-push to mark possible en-passant captures

	fen >> token;
	if (token != "-")
		ep_rear = bit::set(type::sq_of(token));

	// creating hash keys

	key = zobrist::pos_key(*this);
	key_kingpawn = zobrist::kingpawn_key(*this);
	if (!(fen >> token))
		return;

	// setting halfmove- & move-count

	half_cnt = stoi(token);
	fen >> move_cnt;
	move_cnt = move_cnt * 2 - 1 - cl;
}

void board::reset()
{
	// resetting the state of the board

	*this = {};
	for (auto& pc : piece_on) pc = no_piece;
	for (auto& cl : castle_right) for (auto& dr : cl) dr = prohibited;
}

void board::new_move(move new_mv)
{
	// updating the board with a new move

	move_cnt += 1;
	half_cnt += 1;
	last_sq   = prohibited;
	verify_deep(pseudolegal(new_mv));

	// decompressing the move

	move::item mv{ new_mv };
	bit64 sq1{ bit::set(mv.sq1) };
	bit64 sq2{ bit::set(mv.sq2) };
	bool castle{ mv.fl == castle_east || mv.fl == castle_west };

	// if the rook is engaged in the move, castling rights have to be adjusted

	if (mv.pc == rook) new_castle_right(cl,   mv.sq1);
	if (mv.vc == rook) new_castle_right(cl_x, mv.sq2);

	// removing the captured piece from the board if necessary
	
	if (mv.vc != no_piece)
	{
		if (mv.fl == enpassant)
		{
			// capturing en-passant

			bit64 vc{ bit::shift(sq2, shift::push1x[cl_x]) };

			verify(ep_rear == sq2);
			verify(vc & pieces[pawn] & side[cl_x]);

			pieces[pawn] &= ~vc;
			side[cl_x] &= ~vc;

			square sq{ bit::scan(vc) };
			verify(piece_on[sq] == pawn);
			piece_on[sq] = no_piece;

			key ^= zobrist::key_pc[cl_x][pawn][sq];
			key_kingpawn ^= zobrist::key_pc[cl_x][pawn][sq];
		}
		else
		{
			// capturing normally

			verify(sq2 & pieces[mv.vc] & side[cl_x]);
			pieces[mv.vc] &= ~sq2;
			side[cl_x]    &= ~sq2;
			key ^= zobrist::key_pc[cl_x][mv.vc][mv.sq2];

			if (mv.vc == pawn)
				key_kingpawn ^= zobrist::key_pc[cl_x][pawn][mv.sq2];
		}

		half_cnt = 0;
		last_sq = mv.sq2;
	}

	// resetting the en-passant square

	if (ep_rear)
	{
		file ep_file{ type::fl_of(bit::scan(ep_rear)) };
		if (pieces[pawn] & side[cl] & bit::ep_adjacent[cl_x][ep_file])
			key ^= zobrist::key_ep[ep_file];
		ep_rear = 0ULL;
	}

	// pawn moves have to be taken special care of
	
	if (mv.pc == pawn)
	{
		// adjusting the half-move-count & the pawn hash key with every pawn move

		half_cnt = 0;
		key_kingpawn ^= zobrist::key_pc[cl][pawn][mv.sq1];
		key_kingpawn ^= zobrist::key_pc[cl][pawn][mv.sq2];

		// double-pushing pawn

		if (std::abs(mv.sq1 - mv.sq2) == 16)
		{
			verify(mv.vc == no_piece);
			square ep{ mv.sq2 ^ 8 };
			ep_rear = bit::set(ep);

			if (pieces[pawn] & side[cl_x] & bit::ep_adjacent[cl][type::fl_of(ep)])
				key ^= zobrist::key_ep[type::fl_of(ep)];
		}
	}

	// adjusting castling squares

	if (castle)
	{
		mv.sq2 = move::king_target[cl][mv.fl];
		sq2 = bit::set(mv.sq2);
	}

	// rearranging the pieces

	pieces[mv.pc] ^= sq1;
	pieces[mv.pc] |= sq2;

	side[cl] ^= sq1;
	side[cl] |= sq2;

	piece_on[mv.sq1] = no_piece;
	piece_on[mv.sq2] = mv.pc;

	key ^= zobrist::key_pc[cl][mv.pc][mv.sq1];
	key ^= zobrist::key_pc[cl][mv.pc][mv.sq2];

	// adjusting the rook when castling

	if (castle)
		adjust_rook(mv.fl);

	// promoting pawns

	if (mv.fl >= promo_knight)
	{
		piece promo_pc{ mv.promo_pc() };

		verify(pieces[pawn] & sq2);
		verify(side[cl] & sq2);
		verify(piece_on[mv.sq2] == pawn);

		pieces[pawn] ^= sq2;
		pieces[promo_pc] |= sq2;
		piece_on[mv.sq2] = promo_pc;

		key ^= zobrist::key_pc[cl][pawn][mv.sq2];
		key ^= zobrist::key_pc[cl][promo_pc][mv.sq2];
		key_kingpawn ^= zobrist::key_pc[cl][pawn][mv.sq2];
	}

	// adjusting the castling rights when the king moves

	if (sq2 & pieces[king])
	{
		sq_king[cl] = mv.sq2;
		for (auto dr : { castle_east, castle_west })
		{
			if (castle_right[cl][dr] != prohibited)
			{
				castle_right[cl][dr] = prohibited;
				key ^= zobrist::key_castle[cl][dr];
			}
		}
		key_kingpawn ^= zobrist::key_pc[cl][king][mv.sq1];
		key_kingpawn ^= zobrist::key_pc[cl][king][mv.sq2];
	}

	// updating side to move

	side[both] = side[white] | side[black];
	cl   ^= 1;
	cl_x ^= 1;
	key  ^= zobrist::key_cl;

	verify(cl == (cl_x ^ 1));
	verify((side[white] | side[black]) == (side[white] ^ side[black]));

	verify_deep(zobrist::pos_key(*this) == key);
	verify_deep(zobrist::kingpawn_key(*this) == key_kingpawn);
}

square board::castling_rook(color cl, direction dr) const
{
	// finding the square of the castling rook (which is not obvious in FRC)
	// assuming that castling rights allow the move in the first place

	verify(type::cl(cl));
	verify(dr == direction::east || dr == direction::west);

	square sq{ move::rook_origin[cl][(int(dr) + 1) / 2] };
	for (; piece_on[sq] != rook; sq = sq - int(dr));
	return sq;
}

void board::adjust_rook(flag fl)
{
	// moving the rook to its castling destination
	// assuming the king has already moved to its destination square

	verify(fl == castle_east || fl == castle_west);

	square sq1{ castle_right[cl][fl] };
	square sq2{ move::rook_target[cl][fl] };
	bit64 sq1_bit{ bit::set(sq1) };
	bit64 sq2_bit{ bit::set(sq2) };

	verify(piece_on[sq1] == rook || piece_on[sq1] == king);

	pieces[rook] ^= sq1_bit;
	pieces[rook] |= sq2_bit;

	if (piece_on[sq1] == rook)
	{
		side[cl] ^= sq1_bit;
		piece_on[sq1] = no_piece;
	}

	side[cl] |= sq2_bit;
	piece_on[sq2] = rook;

	key ^= zobrist::key_pc[cl][rook][sq1];
	key ^= zobrist::key_pc[cl][rook][sq2];
}

void board::new_castle_right(color cl, square sq)
{
	// updating the right to castle if a rook is engaged in the move

	verify(type::cl(cl));
	verify(type::sq(sq));

	for (flag fl : { castle_east, castle_west})
	{
		if (castle_right[cl][fl] == sq)
		{
			castle_right[cl][fl] = prohibited;
			key ^= zobrist::key_castle[cl][fl];
			break;
		}
	}
}

void board::null_move(bit64& ep, square& sq)
{
	// doing a "null move" by not moving but pretending to have moved, used for null move pruning
	// special care for en-passant has to be taken

	if (ep_rear)
	{
		file fl_ep{ type::fl_of(bit::scan(ep_rear)) };
		if (pieces[pawn] & side[cl] & bit::ep_adjacent[cl_x][fl_ep])
			key ^= zobrist::key_ep[fl_ep];
		ep = ep_rear;
		ep_rear = 0ULL;
	}

	sq = last_sq;
	half_cnt += 1;
	move_cnt += 1;
	key  ^= zobrist::key_cl;
	cl   ^= 1;
	cl_x ^= 1;

	verify_deep(zobrist::pos_key(*this) == key);
}

void board::revert_null_move(bit64& ep, square& sq)
{
	// undoing the "null move"

	if (ep)
	{
		file fl_ep{ type::fl_of(bit::scan(ep)) };
		if (pieces[pawn] & side[cl_x] & bit::ep_adjacent[cl][fl_ep])
			key ^= zobrist::key_ep[fl_ep];
		ep_rear = ep;
	}

	last_sq = sq;
	half_cnt -= 1;
	move_cnt -= 1;
	key  ^= zobrist::key_cl;
	cl   ^= 1;
	cl_x ^= 1;

	verify_deep(zobrist::pos_key(*this) == key);
}

void board::display() const
{
	// displaying the board

	std::string row{ "+---+---+---+---+---+---+---+---+" };
	std::cout << row << "\n";
	for (square sq{ a8 }; sq >= h1; sq = sq - 1)
	{
		char pc{ "PNBRQK "[piece_on[sq]] };
		std::cout << "| "
			<< (bit::set(sq) & side[black] ? char(tolower(pc)) : pc) << " "
			<< (sq % 8 ? "" : "|\n" + row + "\n");
	}
}

bool board::check() const
{
	// returning true if the moving side is in check

	return !attack::check(*this, cl, pieces[king] & side[cl]);
}

bool board::gives_check(move m) const
{
	// determining if the move gives check (assuming it is a legal move)

	verify_deep(pseudolegal(m));

	move::item mv{ m };
	bit64 occ{ side[both] };

	if (mv.castling())
	{
		// considering only the rook when castling

		occ ^= bit::set(mv.sq1);
		mv.pc = rook;
		mv.sq1 = mv.sq2;
		mv.sq2 = move::rook_target[mv.cl][mv.fl];
	}

	bit64 sq1{ bit::set(mv.sq1) };
	bit64 sq2{ bit::set(mv.sq2) };

	occ ^= sq1;
	occ |= sq2;

	// direct check

	if (mv.promo())
		mv.pc = mv.promo_pc();

	if (attack::by_piece(mv.pc, mv.sq2, mv.cl, occ) & pieces[king] & side[cl_x])
		return true;

	// discovered check

	if (mv.fl == enpassant)
		occ &= ~(sq1 | bit::shift(ep_rear, shift::push1x[cl_x]));

	return (attack::by_slider<bishop>(sq_king[cl_x], occ) & (pieces[bishop] | pieces[queen]) & occ & side[cl])
		|| (attack::by_slider<rook>(sq_king[cl_x], occ)   & (pieces[rook]   | pieces[queen]) & occ & side[cl]);
}

bool board::recapture(move mv) const
{
	// returning true if the move is a recapture

	return mv.sq2() == last_sq;
}

bool board::lone_pawns() const
{
	// checking whether there are only pawns left with the king

	return side[both] == (pieces[king] | pieces[pawn]);
}

bool board::lone_knights() const
{
	// checking whether there are only knights left with the king

	return side[both] == (pieces[king] | pieces[knight]);
}

bool board::lone_bishops() const
{	
	// checking whether there are only bishops left with the king

	return side[both] == (pieces[king] | pieces[bishop]);
}

bool board::repetition(const std::array<key64, 256>& hash, int offset) const
{
	// marking every one-fold-repetition as a draw
	// the hash array has a big safety margin, 100 + lim::dt would be enough

	verify(hash[offset] == key);

	for (int i{ 4 }; i <= half_cnt && i <= offset; i += 2)
	{
		if (hash[offset - i] == hash[offset])
			return true;
	}
	return false;
}

bool board::draw(std::array<key64, 256>& hash, int offset) const
{
	// detecting draw-by-50-move-rule & draw-by-repetition
	// draw-by-insufficient-mating-material is handled by the static evaluation

	verify(offset < hash.size());
	hash[offset] = key;
	return repetition(hash, offset) || half_cnt >= 100;
}

bool board::pseudolegal(move m) const
{
	// asserting the correct match between the board and a move
	// a correct move format is assumed

	if (!m)
		return false;

	move::item mv{ m };
	bit64 sq1{ bit::set(mv.sq1) };
	bit64 sq2{ bit::set(mv.sq2) };

	// assessing the side to move

	if (mv.cl != cl)
		return false;

	// assessing start square sq1

	if (!(sq1 & (pieces[mv.pc] & side[mv.cl])))
		return false;

	// assessing end square sq2

	if (mv.castling())
	{
		if (!(sq2 & (pieces[rook] & side[mv.cl])))
			return false;
	}
	else if (mv.vc == no_piece || mv.fl == enpassant)
	{
		if (piece_on[mv.sq2] != no_piece)
			return false;
	}
	else
	{
		verify(mv.vc != no_piece);
		if (!(sq2 & (pieces[mv.vc] & side[mv.cl ^ 1])))
			return false;
	}

	// checking whether the path between sq1 & sq2 is free

	switch (mv.pc)
	{
	case pawn:
		if (std::abs(mv.sq1 - mv.sq2) == 16)
			return piece_on[(mv.sq1 + mv.sq2) / 2] == no_piece;
		else if (mv.fl == enpassant)
			return ep_rear == sq2;
		else
			return true;

	case knight:
		return true;

	case bishop:
		return sq2 & attack::by_slider<bishop>(mv.sq1, side[both]);

	case rook:
		return sq2 & attack::by_slider<rook>(mv.sq1, side[both]);

	case queen:
		return sq2 & attack::by_slider<queen>(mv.sq1, side[both]);

	case king:
		if (mv.fl == no_flag)
			return true;
		else
		{
			// assessing the right to castle

			verify(mv.castling());
			if (castle_right[cl][mv.fl] == prohibited)
				return false;

			square king_sq1{ sq_king[cl] };
			square king_sq2{ move::king_target[cl][mv.fl] };
			square rook_sq1{ castle_right[cl][mv.fl] };
			square rook_sq2{ move::rook_target[cl][mv.fl] };
			square sq_max{}, sq_min{};

			if (mv.fl == castle_east)
			{
				sq_max = std::max(king_sq1, rook_sq2);
				sq_min = std::min(king_sq2, rook_sq1);
			}
			else
			{
				sq_max = std::max(king_sq2, rook_sq1);
				sq_min = std::min(king_sq1, rook_sq2);
			}

			bit64 occ{side[both] ^ (bit::set(king_sq1) | bit::set(rook_sq1)) };
			if (!(bit::between[sq_max][sq_min] & occ))
			{
				bit64  king_path{ bit::between[king_sq1][king_sq2] };
				while (king_path)
				{
					square sq{ bit::scan(king_path) };
					if (attack::sq(*this, sq, occ) & side[cl_x])
						return false;
					king_path &= king_path - 1;
				}
				return true;
			}
			else
				return false;
		}

	default:
		verify(false);
		return false;
	}
}

bool board::legal(move mv) const
{
	// checking for full legality of the move
	// this is quiet expensive to do and only used to check moves from the opening book

	if (!pseudolegal(mv))
		return false;

	board new_pos{ *this };
	new_pos.new_move(mv);
	return attack::check(new_pos, new_pos.cl_x, new_pos.pieces[king] & new_pos.side[new_pos.cl_x]);
}

bool board::legal() const
{
	// checking if the last move was legally made
	// king moves and castling moves are always legal and don't have to be checked

	return attack::check(*this, cl_x, pieces[king] & side[cl_x]);
}