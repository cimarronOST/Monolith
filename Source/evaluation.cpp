/*
  Monolith 0.1  Copyright (C) 2017 Jonas Mayr

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


#include "bitboard.h"
#include "game.h"
#include "evaluation.h"

namespace
{
	const int phase_max{ 24 };
	int phase_weight[phase_max + 1];

	const int negate[]{ 1, -1 };
}

void eval::init()
{
	for (int i{ 0 }; i <= phase_max; ++i)
		phase_weight[i] = i * 256 / phase_max;
}

int eval::eval_board(pos &board)
{
	return (material(board) + piece_square(board)) * negate[board.turn];
}
int eval::material(pos &board)
{
	int count{ 0 };
	for (int piece{ PAWNS }; piece <= QUEENS; ++piece)
	{
		count += value[piece] *
			(bb::popcnt(board.pieces[piece] & board.side[WHITE])
			-bb::popcnt(board.pieces[piece] & board.side[BLACK]));
	}
	return count;
}
int eval::piece_square(pos &board)
{
	int sum[2][2]{ };

	for (int color{ WHITE }; color <= BLACK; ++color)
	{
		uint64 pieces{ board.side[color] };
		while (pieces)
		{
			bb::bitscan(pieces);
			int piece{ board.piece_sq[bb::lsb()] };

			sum[MG][color] += p_s_table[piece][MG][color][63 - bb::lsb()];
			sum[EG][color] += p_s_table[piece][EG][color][63 - bb::lsb()];

			pieces &= pieces - 1;
		}
	}

	assert(board.phase >= 0);
	int phase{ board.phase <= phase_max ? board.phase : phase_max };

	int weight{ phase_weight[phase] };
	int mg_score{ sum[MG][WHITE] - sum[MG][BLACK] };
	int eg_score{ sum[EG][WHITE] - sum[EG][BLACK] };

	return (mg_score * weight + eg_score * (256 - weight)) >> 8;
}