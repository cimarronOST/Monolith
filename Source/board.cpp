/*
  Monolith Copyright (C) 2017-2026 Jonas Mayr

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
#include <cctype>
#include <string>

#include "main.h"
#include "types.h"
#include "attack.h"
#include "zobrist.h"
#include "bit.h"
#include "move.h"
#include "board.h"

void board::parse_fen(const std::string& fen_string)
{
	// conveying a FEN-string into the internal board representation
	// a correct FEN-string is assumed

	reset();
	square sq{ H1 };
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
            color sd{ islower(ch) ? BLACK : WHITE };
			char  ch_upper{ char(toupper(ch)) };

            for (piece pc : { PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING })
			{
				if (ch_upper != "PNBRQK"[pc])
					continue;

				pieces[pc] |= bit::set(sq);
				side[sd] |= bit::set(sq);
				piece_on[sq] = pc;
				break;
			}
			sq += 1;
		}
	}
	verify(sq == A8 + 1);
    verify(!(side[WHITE] & side[BLACK]));

	// determining squares of the kings

    sq_king[WHITE] = bit::scan(pieces[KING] & side[WHITE]);
    sq_king[BLACK] = bit::scan(pieces[KING] & side[BLACK]);
	verify(type::sq(sq_king[WHITE]) && type::sq(sq_king[BLACK]));

	// determining the side to move

    side[BOTH] = side[WHITE] | side[BLACK];
    fen >> token;
    verify(token == "w" || token == "b");
    cl = token == "w" ? WHITE : BLACK;
    cl_x = cl ^ 1;

	// setting castling rights

	fen >> token;
	if (token != "-")
	{
		for (; !token.empty(); token.pop_back())
		{
			color sd{ islower(token.back()) ? BLACK : WHITE };
			char  ch{ char(toupper(token.back())) };

			if (ch == 'K')
			{
				castle_right[sd][CASTLE_EAST] = castling_rook(sd, direction::EAST);
			}
			else if (ch == 'Q')
			{
				castle_right[sd][CASTLE_WEST] = castling_rook(sd, direction::WEST);
			}
			else if (ch >= 'A' && ch <= 'H')
			{
                file f_rook{ file('H' - ch) };
                file f_king{ type::fl_of(sq_king[sd]) };
                castle_right[sd][f_rook < f_king ? CASTLE_EAST : CASTLE_WEST] = square(f_rook) + (sd == WHITE ? H1 : H8);
			}
		}
	}

	// saving the rear square of a double-push to mark possible en-passant captures

	fen >> token;
	if (token != "-")
		ep_rear = bit::set(type::sq_of(token));

	// creating hash keys & setting half-move- & move-count

	get_keys();
	if (!(fen >> token))
		return;
	half_cnt = stoi(token);
	fen >> move_cnt;
	move_cnt = move_cnt * 2 - 1 - cl;
}

void board::get_keys()
{
	// creating all the hash keys of the current position

	key.pos = zobrist::pos_key(*this);
	key.pawn = zobrist::pawn_key(*this);
	key.minor = zobrist::minor_key(*this);
	key.major = zobrist::major_key(*this);
    key.nonpawn[WHITE] = zobrist::nonpawn_key(*this, WHITE);
    key.nonpawn[BLACK] = zobrist::nonpawn_key(*this, BLACK);
}

void board::reset()
{
	// resetting the state of the board

	*this = {};
	for (auto& pc : piece_on) pc = NO_PIECE;
	for (auto& sd : castle_right) for (auto& dr : sd) dr = NO_SQUARE;
}

void board::new_move(move new_mv)
{
	// updating the board with a new move

	move_cnt += 1;
	half_cnt += 1;
	last_sq   = NO_SQUARE;

	// decompressing the move

	move::item mv{ new_mv };
	bit64 sq1{ bit::set(mv.sq1) };
	bit64 sq2{ bit::set(mv.sq2) };
	bool castle{ mv.castling() };

	// if the rook is engaged in the move, castling rights have to be adjusted

    if (mv.pc == ROOK) rook_castle_right(cl,   mv.sq1);
    if (mv.vc == ROOK) rook_castle_right(cl_x, mv.sq2);

	// removing the captured piece from the board if necessary

	if (mv.vc != NO_PIECE)
	{
		remove_piece(mv, sq2);
		half_cnt = 0;
		last_sq = mv.sq2;
	}

	// resetting the en-passant square

    if (ep_rear)
	{
		file ep_file{ type::fl_of(bit::scan(ep_rear)) };
        if (pieces[PAWN] & side[cl] & bit::ep_adjacent[cl_x][ep_file])
			key.pos ^= zobrist::key_ep[ep_file];
		ep_rear = 0ULL;
	}

	// pawn moves have to be taken special care of
	
    if (mv.pc == PAWN)
	{
		half_cnt = 0;
		pawn_push(mv);
	}

	// adjusting castling squares

	if (castle)
	{
		mv.sq2 = move::king_target[cl][mv.fl];
		sq2 = bit::set(mv.sq2);
	}

	// rearranging the pieces, adjusting the rook when castling, promoting pawns

	rearrange_pieces(mv, sq1, sq2);

	if (castle)
		adjust_rook(mv.fl);

	if (mv.promo())
		promote_pawn(mv, sq2);

	// adjusting the castling rights when the king moves

    if (mv.pc == KING)
	{
		sq_king[cl] = mv.sq2;
		king_castle_right();
	}

	// updating side to move

    side[BOTH] = side[WHITE] | side[BLACK];
	cl   ^= 1;
	cl_x ^= 1;
	key.pos ^= zobrist::key_cl;

    verify(cl == (cl_x ^ 1));
    verify((side[WHITE] | side[BLACK]) == (side[WHITE] ^ side[BLACK]));
}

square board::castling_rook(color sd, direction dr) const
{
	// finding the square of the castling rook (which is not obvious in FRC)
	// assuming that castling rights allow the move in the first place

	verify(type::cl(sd));
	verify(dr == direction::EAST || dr == direction::WEST);

	square sq{ move::rook_origin[sd][(int(dr) + 1) / 2] };
	for (; piece_on[sq] != ROOK; sq = sq - int(dr));
	return sq;
}

void board::pawn_push(move::item& mv)
{
	// adjusting the pawn hash key with every pawn move

	key.pawn ^= zobrist::key_pc[cl][PAWN][mv.sq1];
	key.pawn ^= zobrist::key_pc[cl][PAWN][mv.sq2];

	// double-pushing pawn

	if (std::abs(mv.sq1 - mv.sq2) == 16)
	{
		verify(mv.vc == NO_PIECE);
		square ep{ mv.sq2 ^ 8 };
		ep_rear = bit::set(ep);

		if (pieces[PAWN] & side[cl_x] & bit::ep_adjacent[cl][type::fl_of(ep)])
			key.pos ^= zobrist::key_ep[type::fl_of(ep)];
	}
}

void board::adjust_rook(flag fl)
{
	// moving the rook to its castling destination
	// assuming the king has already moved to its destination square

	verify(fl == CASTLE_EAST || fl == CASTLE_WEST);

	square sq1{ castle_right[cl][fl] };
	square sq2{ move::rook_target[cl][fl] };
	bit64 sq1_bit{ bit::set(sq1) };
	bit64 sq2_bit{ bit::set(sq2) };

	verify(piece_on[sq1] == ROOK || piece_on[sq1] == KING);

	pieces[ROOK] ^= sq1_bit;
	pieces[ROOK] |= sq2_bit;

	if (piece_on[sq1] == ROOK)
	{
		side[cl] ^= sq1_bit;
		piece_on[sq1] = NO_PIECE;
	}

	side[cl] |= sq2_bit;
	piece_on[sq2] = ROOK;
	
	key.pos         ^= zobrist::key_pc[cl][ROOK][sq1];
	key.pos         ^= zobrist::key_pc[cl][ROOK][sq2];
	key.major       ^= zobrist::key_pc[cl][ROOK][sq1];
	key.major       ^= zobrist::key_pc[cl][ROOK][sq2];
	key.nonpawn[cl] ^= zobrist::key_pc[cl][ROOK][sq1];
	key.nonpawn[cl] ^= zobrist::key_pc[cl][ROOK][sq2];
}

void board::rook_castle_right(color sd, square sq)
{
	// updating the right to castle if a rook is engaged in the move

	verify(type::cl(sd));
	verify(type::sq(sq));

	for (flag fl : { CASTLE_EAST, CASTLE_WEST})
	{
		if (castle_right[sd][fl] == sq)
		{
			castle_right[sd][fl] = NO_SQUARE;
			key.pos ^= zobrist::key_castle[sd][fl];
			break;
		}
	}
}

void board::king_castle_right()
{
	// updating the right to castle if the king moves

	for (auto dr : { CASTLE_EAST, CASTLE_WEST })
	{
		if (castle_right[cl][dr] != NO_SQUARE)
		{
			castle_right[cl][dr] = NO_SQUARE;
			key.pos ^= zobrist::key_castle[cl][dr];
		}
	}
}

void board::rearrange_pieces(move::item& mv, bit64& sq1, bit64& sq2)
{
	// rearranging the pieces, moving the piece from square 1 to square 2

	pieces[mv.pc] ^= sq1;
	pieces[mv.pc] |= sq2;

	side[cl] ^= sq1;
	side[cl] |= sq2;

	piece_on[mv.sq1] = NO_PIECE;
	piece_on[mv.sq2] = mv.pc;

	key.pos ^= zobrist::key_pc[cl][mv.pc][mv.sq1];
	key.pos ^= zobrist::key_pc[cl][mv.pc][mv.sq2];

	// adjusting the keys accordingly

	if (mv.pc != PAWN)
	{
		key.nonpawn[cl] ^= zobrist::key_pc[cl][mv.pc][mv.sq1];
		key.nonpawn[cl] ^= zobrist::key_pc[cl][mv.pc][mv.sq2];

		if (mv.pc == KNIGHT || mv.pc == BISHOP || mv.pc == KING)
			key.minor ^= zobrist::key_pc[cl][mv.pc][mv.sq1],
			key.minor ^= zobrist::key_pc[cl][mv.pc][mv.sq2];
		else if (mv.pc == ROOK || mv.pc == QUEEN || mv.pc == KING)
			key.major ^= zobrist::key_pc[cl][mv.pc][mv.sq1],
			key.major ^= zobrist::key_pc[cl][mv.pc][mv.sq2];
	}
}

void board::remove_piece(move::item& mv, bit64& sq2)
{
	// removing the captured piece & adjusting keys accordingly

	if (mv.fl == ENPASSANT)
	{
		// capturing en-passant

		bit64 vc{ bit::shift(sq2, shift::push1x[cl_x]) };

		verify(ep_rear == sq2);
		verify(vc & pieces[PAWN] & side[cl_x]);

		pieces[PAWN] &= ~vc;
		side[cl_x]   &= ~vc;

		square sq{ bit::scan(vc) };
		verify(piece_on[sq] == PAWN);
		piece_on[sq] = NO_PIECE;

		key.pos  ^= zobrist::key_pc[cl_x][PAWN][sq];
		key.pawn ^= zobrist::key_pc[cl_x][PAWN][sq];
	}
	else
	{
		// capturing normally

		verify(sq2 & pieces[mv.vc] & side[cl_x]);
		pieces[mv.vc] &= ~sq2;
		side[cl_x] &= ~sq2;
		key.pos ^= zobrist::key_pc[cl_x][mv.vc][mv.sq2];
		key.nonpawn[cl_x] ^= zobrist::key_pc[cl_x][mv.vc][mv.sq2];

		if (mv.vc == PAWN)
			key.pawn  ^= zobrist::key_pc[cl_x][mv.vc][mv.sq2];
		else if (mv.vc == KNIGHT || mv.vc == BISHOP)
			key.minor ^= zobrist::key_pc[cl_x][mv.vc][mv.sq2];
		else if (mv.vc == ROOK   || mv.vc == QUEEN)
			key.major ^= zobrist::key_pc[cl_x][mv.vc][mv.sq2];
	}
}

void board::promote_pawn(move::item& mv, bit64& sq2)
{
	// promoting a pawn

	verify(pieces[PAWN] & sq2);
	verify(side[cl] & sq2);
	verify(piece_on[mv.sq2] == PAWN);

	piece promo_pc{ mv.promo_pc() };
	pieces[PAWN]     ^= sq2;
	pieces[promo_pc] |= sq2;
	piece_on[mv.sq2]  = promo_pc;

	key.pos  ^= zobrist::key_pc[cl][PAWN][mv.sq2];
	key.pawn ^= zobrist::key_pc[cl][PAWN][mv.sq2];
	key.pos  ^= zobrist::key_pc[cl][promo_pc][mv.sq2];
	key.nonpawn[cl] ^= zobrist::key_pc[cl][promo_pc][mv.sq2];

	if (promo_pc == KNIGHT || promo_pc == BISHOP)
		key.minor ^= zobrist::key_pc[cl][promo_pc][mv.sq2];
	else if (promo_pc == ROOK || promo_pc == QUEEN)
		key.major ^= zobrist::key_pc[cl][promo_pc][mv.sq2];
}

void board::null_move(bit64& ep, square& sq)
{
	// doing a "null move" by not moving but pretending to have moved, used for null move pruning
	// special care for en-passant has to be taken

	if (ep_rear)
	{
		file fl_ep{ type::fl_of(bit::scan(ep_rear)) };
		if (pieces[PAWN] & side[cl] & bit::ep_adjacent[cl_x][fl_ep])
			key.pos ^= zobrist::key_ep[fl_ep];
		ep = ep_rear;
		ep_rear = 0ULL;
	}

	sq = last_sq;
	half_cnt += 1;
	move_cnt += 1;
	key.pos  ^= zobrist::key_cl;
	cl   ^= 1;
	cl_x ^= 1;

	verify(zobrist::pos_key(*this) == key.pos);
}

void board::revert_null_move(bit64& ep, square& sq)
{
	// undoing the "null move"

	if (ep)
	{
		file fl_ep{ type::fl_of(bit::scan(ep)) };
		if (pieces[PAWN] & side[cl_x] & bit::ep_adjacent[cl][fl_ep])
			key.pos ^= zobrist::key_ep[fl_ep];
		ep_rear = ep;
	}

	last_sq = sq;
	half_cnt -= 1;
	move_cnt -= 1;
	key.pos  ^= zobrist::key_cl;
	cl   ^= 1;
	cl_x ^= 1;

	verify(zobrist::pos_key(*this) == key.pos);
}

void board::display() const
{
	// displaying the board

	std::string row{ "+---+---+---+---+---+---+---+---+" };
	std::cout << row << "\n";
	for (square sq{ A8 }; sq >= H1; sq = sq - 1)
	{
		char pc{ "PNBRQK "[piece_on[sq]] };
		std::cout << "| "
			<< (bit::set(sq) & side[BLACK] ? char(tolower(pc)) : pc) << " "
			<< (sq % 8 ? "" : "|\n" + row + "\n");
	}
}

bool board::check() const
{
	// returning true if the moving side is in check

	return !attack::safe(*this, cl, pieces[KING] & side[cl]);
}

bool board::gives_check(move m) const
{
	// determining if the move gives check (assuming it is a legal move)

	move::item mv{ m };
	bit64 occ{ side[BOTH] };

	if (mv.castling())
	{
		// considering only the rook when castling

		occ ^= bit::set(mv.sq1);
		mv.pc = ROOK;
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

	if (attack::by_piece(mv.pc, mv.sq2, mv.cl, occ) & pieces[KING] & side[cl_x])
		return true;

	// discovered check

	if (mv.fl == ENPASSANT)
		occ &= ~(sq1 | bit::shift(ep_rear, shift::push1x[cl_x]));

	return (attack::by_slider<BISHOP>(sq_king[cl_x], occ) & (pieces[BISHOP] | pieces[QUEEN]) & occ & side[cl])
		|| (attack::by_slider<ROOK>(sq_king[cl_x], occ)   & (pieces[ROOK]   | pieces[QUEEN]) & occ & side[cl]);
}

bool board::recapture(move mv) const
{
	// returning true if the move is a recapture

	return mv.sq2() == last_sq;
}

bool board::lone_pawns() const
{
	// checking whether there are only pawns left with the king

	return side[BOTH] == (pieces[KING] | pieces[PAWN]);
}

bool board::lone_knights() const
{
	// checking whether there are only knights left with the king

	return side[BOTH] == (pieces[KING] | pieces[KNIGHT]);
}

bool board::lone_bishops() const
{	
	// checking whether there are only bishops left with the king

	return side[BOTH] == (pieces[KING] | pieces[BISHOP]);
}

bool board::repetition(const std::array<key64, 256>& hash, int offset) const
{
	// marking every one-fold-repetition as a draw
	// the hash array has a big safety margin, 100 + lim::dt would be enough

	verify(hash[offset] == key.pos);

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

	verify(offset < (int)hash.size());
	hash[offset] = key.pos;
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
		if (!(sq2 & (pieces[ROOK] & side[mv.cl])))
			return false;
	}
	else if (mv.vc == NO_PIECE || mv.fl == ENPASSANT)
	{
		if (piece_on[mv.sq2] != NO_PIECE)
			return false;
	}
	else
	{
		verify(mv.vc != NO_PIECE);
		if (!(sq2 & (pieces[mv.vc] & side[mv.cl ^ 1])))
			return false;
	}

	// checking whether the path between sq1 & sq2 is free

	switch (mv.pc)
	{
	case PAWN:
		if (std::abs(mv.sq1 - mv.sq2) == 16)
			return piece_on[(mv.sq1 + mv.sq2) / 2] == NO_PIECE;
		else if (mv.fl == ENPASSANT)
			return ep_rear == sq2;
		else
			return true;

	case KNIGHT:
		return true;

	case BISHOP:
		return sq2 & attack::by_slider<BISHOP>(mv.sq1, side[BOTH]);

	case ROOK:
		return sq2 & attack::by_slider<ROOK>(mv.sq1, side[BOTH]);

	case QUEEN:
		return sq2 & attack::by_slider<QUEEN>(mv.sq1, side[BOTH]);

	case KING:
		if (mv.fl == NO_FLAG)
			return true;
		else
		{
			// assessing the right to castle

			verify(mv.castling());
			if (castle_right[cl][mv.fl] == NO_SQUARE)
				return false;

			square king_sq1{ sq_king[cl] };
			square king_sq2{ move::king_target[cl][mv.fl] };
			square rook_sq1{ castle_right[cl][mv.fl] };
			square rook_sq2{ move::rook_target[cl][mv.fl] };
			square sq_max{}, sq_min{};

			if (mv.fl == CASTLE_EAST)
			{
				sq_max = std::max(king_sq1, rook_sq2);
				sq_min = std::min(king_sq2, rook_sq1);
			}
			else
			{
				sq_max = std::max(king_sq2, rook_sq1);
				sq_min = std::min(king_sq1, rook_sq2);
			}

			bit64 occ{side[BOTH] ^ (bit::set(king_sq1) | bit::set(rook_sq1)) };
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

[[maybe_unused]] bool board::legal(move mv) const
{
	// checking for full legality of the move, this is quiet expensive to do

	if (!pseudolegal(mv))
		return false;

	board new_pos{ *this };
	new_pos.new_move(mv);
	return attack::safe(new_pos, new_pos.cl_x, new_pos.pieces[KING] & new_pos.side[new_pos.cl_x]);
}

bool board::legal() const
{
	// checking if the last move was legally made
	// king moves and castling moves are always legal and don't have to be checked

	return attack::safe(*this, cl_x, pieces[KING] & side[cl_x]);
}