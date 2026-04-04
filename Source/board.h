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


#pragma once

#include <array>
#include <string>

#include "move.h"
#include "types.h"

// representing the internal chessboard

class board
{
public:
	// representation of the placement of all pieces
	// using both bitboards and a piece-array of all the squares

	std::array<bit64,  6> pieces{};
	std::array<bit64,  3> side{};
	std::array<piece, 64> piece_on{};
	std::array<square, 2> sq_king{};

	// representing all other positional essentials

	int move_cnt{};
	int half_cnt{};
	color cl{};
	color cl_x{};
	bit64 ep_rear{};
	castle_sq castle_right{};
	square last_sq{};

	// adding various positional hash keys

	struct key_pos
	{
		key64 pos{};
		key64 pawn{};
		key64 minor{};
		key64 major{};
		std::array<key64, 2> nonpawn{};
	} key{};

	// parsing a string in FEN-format

	void parse_fen(const std::string& fen_string);
	void get_keys();
	void reset();

	// moving on the board

	void new_move(move new_mv);
	void null_move(bit64& ep, square& sq);
	void revert_null_move(bit64& ep, square& sq);

private:
	square castling_rook(color cl, direction dr) const;
	void pawn_push(move::item& mv);
	void adjust_rook(flag fl);
	void rook_castle_right(color sd, square sq);
	void king_castle_right();
	void rearrange_pieces(move::item& mv, bit64& sq1, bit64& sq2);
	void remove_piece(move::item& mv, bit64& sq2);
	void promote_pawn(move::item& mv, bit64& sq2);

public:
	// checking for various special cases

	void display() const;
	bool check() const;
	bool gives_check(move mv) const;
	bool recapture(move mv) const;

	bool lone_pawns() const;
	bool lone_knights() const;
	bool lone_bishops() const;

	bool repetition(const std::array<key64, 256>& hash, int offset) const;
	bool draw(std::array<key64, 256> &hash, int offset) const;

	// checking for legality

	bool pseudolegal(move mv) const;
	bool legal(move mv) const;
	bool legal() const;
};