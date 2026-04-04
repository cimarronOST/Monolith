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


#include <type_traits>
#include <bit>

#include "main.h"
#include "types.h"
#include "bit.h"
#include "misc.h"
#include "board.h"
#include "move.h"
#include "zobrist.h"

void zobrist::init_keys()
{
	// generating Zobrist hash keys

	rand_64 rand_gen{};
	for (auto&  cl : key_pc)     for (auto& pc  : cl) for (auto& key : pc) key = rand_gen.rand64();
	for (auto&  cl : key_castle) for (auto& key : cl) key = rand_gen.rand64();
	for (auto& key : key_ep)     key = rand_gen.rand64();
	key_cl = rand_gen.rand64();
}

key64 zobrist::pos_key(const board &pos)
{
	// generating a hash key for the current position

	key64 key{};

	// considering all pieces

	for (color cl : { WHITE, BLACK })
	{
		for (bit64 pieces{ pos.side[cl] }; pieces; pieces &= pieces - 1)
		{
			square sq{ bit::scan(pieces) };
			verify(pos.piece_on[sq] != NO_PIECE);
			key ^= key_pc[cl][pos.piece_on[sq]][sq];
		}
	}

	// considering castling rights

	for (color cl : { WHITE, BLACK })
	{
		for (flag i : { CASTLE_EAST, CASTLE_WEST })
			if (pos.castle_right[cl][i] != NO_SQUARE)
				key ^= key_castle[cl][i];
	}

	// considering en-passant square only if a capturing pawn stands ready

	if (pos.ep_rear)
	{
		file ep_file{ type::fl_of(bit::scan(pos.ep_rear)) };
		if (pos.pieces[PAWN] & pos.side[pos.cl] & bit::ep_adjacent[pos.cl_x][ep_file])
			key ^= key_ep[ep_file];
	}

	// considering side to move

	key ^= pos.cl == BLACK ? key_cl : 0ULL;
	return key;
}

key64 zobrist::pos_key(const board& pos, move& new_mv)
{
	// generating a new hash key simulating the state as if the move had been made
	// en-passant and changes to castling rights are not considered

	key64 key{ pos.key.pos };
	move::item mv{ new_mv };

	// considering the moving piece

	key ^= key_pc[mv.cl][mv.pc][mv.sq1];
	key ^= key_pc[mv.cl][mv.promo() ? mv.promo_pc() : mv.pc][mv.sq2];

	// considering the captured piece

	if (mv.vc != NO_PIECE)
		key ^= key_pc[mv.cl ^ 1][mv.vc][mv.sq2];

	return key;
}

key64 zobrist::adjust_key(const key64& key, move& mv)
{
	// adjusting the hash key based on a singular extension move

	return key ^ key64(mv.raw());
}

key64 zobrist::pawn_key(const board& pos)
{
	// generating a hash key for the current position only considering pawns

	key64 key{};
	for (color cl : { WHITE, BLACK })
	{
		for (bit64 pawns{ pos.pieces[PAWN] & pos.side[cl] }; pawns; pawns &= pawns - 1)
			key ^= key_pc[cl][PAWN][bit::scan(pawns)];
	}
	return  key;
}

key64 zobrist::nonpawn_key(const board& pos, color cl)
{
	// generating a hash key for the current position considering all pieces except pawns

	key64 key{};
	for (bit64 pieces{ pos.side[cl] }; pieces; pieces &= pieces - 1)
	{
		square sq{ bit::scan(pieces) };
		verify(pos.piece_on[sq] != NO_PIECE);
		if (pos.piece_on[sq] != PAWN)
			key ^= key_pc[cl][pos.piece_on[sq]][sq];
	}
	return key;
}

key64 zobrist::kingpawn_key(const board& pos)
{
	// adding the king position to the pawn hash key to index the king-pawn evaluation table

	return pos.key.pawn ^ key_pc[WHITE][KING][pos.sq_king[WHITE]] ^ key_pc[BLACK][KING][pos.sq_king[BLACK]];
}

key64 zobrist::minor_key(const board& pos)
{
	// generating a hash key considering knights, bishops and kings

	key64 key{};
	for (color cl : { WHITE, BLACK })
	{
		for (piece pc : { KNIGHT, BISHOP, KING })
			for (bit64 minor{ pos.pieces[pc] & pos.side[cl] }; minor; minor &= minor - 1)
				key ^= key_pc[cl][pc][bit::scan(minor)];
	}
	return key;
}

key64 zobrist::major_key(const board& pos)
{
	// generating a hash key considering rooks, queens and kings

	key64 key{};
	for (color cl : { WHITE, BLACK })
	{
		for (piece pc : { ROOK, QUEEN, KING })
			for (bit64 major{ pos.pieces[pc] & pos.side[cl] }; major; major &= major - 1)
				key ^= key_pc[cl][pc][bit::scan(major)];
	}
	return key;
}

template key64 zobrist::mat_key<board>(board&, bool);
template key64 zobrist::mat_key<piece_list>(piece_list&, bool);
template<typename pieces>
key64 zobrist::mat_key(pieces& p, bool mirror)
{
	// generating a material signature key from a piece-list or board position, used to probe Syzygy table-bases

	key64 key{};
	int pop{};
    color cl{ mirror ? BLACK : WHITE };
    for (color cl_key : { WHITE, BLACK })
	{
        for (piece pc : { PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING })
		{
			if constexpr (std::is_same<pieces, board>::value)
				 pop = std::popcount(p.pieces[pc] & p.side[cl]);
			else pop = p[cl][pc];
			for (; pop > 0; --pop)
				key ^= key_pc[cl_key][pc][pop - 1];
		}
		cl ^= 1;
	}
	return key;
}

key64 zobrist::mat_key(uint8 piece[], int cnt)
{
	// generating a material signature key from a piece list, used to probe table-bases
	// the list has to be converted first in order to generate a key
	// syzygy encoding: 1-6 for white pawn-king, 9-14 for black pawn-king

	piece_list pc_list{};
	for (int i{}; i < cnt; ++i)
	{
		if (piece[i])
			pc_list[piece[i] >> 3][(piece[i] & 7) - 1] += 1;
	}

	return mat_key<piece_list>(pc_list, false);
}
