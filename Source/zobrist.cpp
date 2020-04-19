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


#include "bit.h"
#include "random.h"
#include "zobrist.h"

std::array<std::array<std::array<key64, 64>, 6>, 2> zobrist::key_pc{};
std::array<std::array<key64, 2>, 2> zobrist::key_castle{};
std::array<key64, 8> zobrist::key_ep{};
key64 zobrist::key_cl{};

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

	for (color cl : { white, black})
	{
		bit64 pieces{ pos.side[cl] };
		while (pieces)
		{
			square sq{ bit::scan(pieces) };
			verify(pos.piece_on[sq] != no_piece);
			key ^= key_pc[cl][pos.piece_on[sq]][sq];
			pieces &= pieces - 1;
		}
	}

	// considering castling rights

	for (color cl : {white, black})
		for (flag i : {castle_east, castle_west})
		{
			if (pos.castle_right[cl][i] != prohibited)
				key ^= key_castle[cl][i];
		}

	// considering en-passant square only if a capturing pawn stands ready

	if (pos.ep_rear)
	{
		file ep_file{ type::fl_of(bit::scan(pos.ep_rear)) };
		if (pos.pieces[pawn] & pos.side[pos.cl] & bit::ep_adjacent[pos.cl_x][ep_file])
			key ^= key_ep[ep_file];
	}

	// considering side to move

	key ^= pos.cl == black ? key_cl : 0ULL;
	return key;
}

key64 zobrist::pos_key(const board& pos, move& new_mv)
{
	// generating a new hash key simulating the state as if the move had been made
	// en-passant and changes to castling rights are not considered

	key64 key{ pos.key };
	move::item mv{ new_mv };

	// considering the moving piece

	key ^= key_pc[mv.cl][mv.pc][mv.sq1];
	key ^= key_pc[mv.cl][mv.promo() ? mv.promo_pc() : mv.pc][mv.sq2];

	// considering the captured piece

	if (mv.vc != no_piece)
		key ^= key_pc[mv.cl ^ 1][mv.vc][mv.sq2];

	return key;
}

key64 zobrist::adjust_key(const key64& key, move& mv)
{
	// adjusting the key based on a new move

	return key ^ key64(mv.raw());
}

key64 zobrist::kingpawn_key(const board& pos)
{
	// generating a hash key for the current position only considering kings and pawns

	key64 key{};
	for (color cl : {white, black})
	{
		key ^= key_pc[cl][king][pos.sq_king[cl]];
		bit64 pawns{ pos.pieces[pawn] & pos.side[cl] };
		while (pawns)
		{
			square sq{ bit::scan(pawns) };
			key ^= key_pc[cl][pawn][sq];
			pawns &= pawns - 1;
		}
		
	}
	return key;
}

template key64 zobrist::mat_key<board>(board&, bool);
template key64 zobrist::mat_key<piece_list>(piece_list&, bool);

template<typename pieces>
key64 zobrist::mat_key(pieces& p, bool mirror)
{
	// generating a material signature key from a piece-list or board position, used to probe Syzygy tablebases

	key64 key{};
	int pop{};
	color cl{ mirror ? black : white };
	for (color cl_key : {white, black})
	{
		for (piece pc : {pawn, knight, bishop, rook, queen, king})
		{
			if constexpr (std::is_same<pieces, board>::value)
				 pop = bit::popcnt(p.pieces[pc] & p.side[cl]);
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
	// generating a material signature key from a piece list, used to probe tablebases
	// the list has to be converted first in order to generate a key
	// syzygy encoding: 1-6 for white pawn-king, 9-14 for black pawn-king

	piece_list pc_list{};
	for (int i{}; i < cnt; ++i)
		if (piece[i])
			pc_list[piece[i] >> 3][(piece[i] & 7) - 1] += 1;

	return mat_key<piece_list>(pc_list, false);
}

key32 zobrist::mv_key(move mv, key64& pos_key)
{
	// generating a move-hash-key, used to hash concurrently searched moves

	return key32(pos_key) ^ (mv.raw() * 1664525U + 1013904223U);
}