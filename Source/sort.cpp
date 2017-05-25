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


#include "sort.h"

namespace
{
	const int p_value[]{ 1, 5, 3, 3, 9, 0, 0, 1 };

	const int max_score{ 0x7fffffff };
	const int base_score{ 1 << 30 };
}

int sort::mvv_lva(pos &board, uint16 move) const
{
	assert(to_flag(move) <= ENPASSANT && to_flag(move) != NONE);

	return p_value[to_flag(move)] * 100 - p_value[board.piece_sq[to_sq1(move)]];
}
int sort::mvv_lva_promo(pos &board, uint16 move) const
{
	assert(to_flag(move) >= PROMO_ROOK);

	int victim{ p_value[board.piece_sq[to_sq2(move)]] + p_value[to_flag(move) - 11] - 2 };
	return victim * 100 - p_value[to_flag(move) - 11];
}

void sort::sort_capt(pos &board, movegen &gen)
{
	for (int nr{ 0 }; nr < gen.capt_cnt; ++nr)
		score_list[nr] = base_score + mvv_lva(board, gen.list[nr]);

	for (int nr{ gen.capt_cnt }; nr < gen.capt_cnt + gen.promo_cnt; ++nr)
		score_list[nr] = base_score + mvv_lva_promo(board, gen.list[nr]);
}

void sort::sort_main(pos &board, movegen &gen, uint16 *best_move, int history[][6][64], uint16 killer[][2], int depth)
{
	static_assert(NONE << 12 == 0x6000, "killer encoding");
	move_cnt = gen.move_cnt;

	// captures + promotions

	sort_capt(board, gen);

	// history

	for (int i{ gen.capt_cnt + gen.promo_cnt }; i < move_cnt; ++i)
	{
		score_list[i] = history[board.turn][board.piece_sq[to_sq1(gen.list[i])]][to_sq2(gen.list[i])];
	}

	// killer moves

	for (int slot{ 0 }; slot < 2; ++slot)
	{
		auto killer_move{ killer[depth][slot] };
		auto piece{ to_flag(killer_move) };

		if (piece <= KINGS)
		{
			killer_move = (killer_move & 0xfff) | 0x6000;
			if (piece != board.piece_sq[to_sq1(killer_move)])
				continue;
		}

		for (int i{ gen.capt_cnt + gen.promo_cnt }; i < move_cnt; ++i)
		{
			if (gen.list[i] == killer_move)
			{
				score_list[i] = base_score + 2 - slot;
				break;
			}
		}
	}

	// pv-/hash-move

	if (best_move != nullptr)
		score_list[best_move - gen.list] = max_score;
}
void sort::sort_root(pos &board, movegen &gen, uint16 *best_move, int history[][6][64])
{
	move_cnt = gen.move_cnt;

	// captures + promotions

	sort_capt(board, gen);

	// history

	for (int i{ gen.capt_cnt + gen.promo_cnt }; i < move_cnt; ++i)
	{
		score_list[i] = history[board.turn][board.piece_sq[to_sq1(gen.list[i])]][to_sq2(gen.list[i])];
	}

	// pv-move

	if (best_move != nullptr)
		score_list[best_move - gen.list] = max_score;
}
void sort::sort_qsearch(pos &board, movegen &gen)
{
	assert(gen.capt_cnt + gen.promo_cnt == gen.move_cnt);
	move_cnt = gen.move_cnt;

	sort_capt(board, gen);
}

uint16 sort::next(movegen &gen)
{
	int nr{ -1 };

	for (int i{ 0 }, best_score{ 0 }; i < move_cnt; ++i)
	{
		if (score_list[i] > best_score)
		{
			best_score = score_list[i];
			nr = i;
		}
	}

	if (nr != -1)
	{
		score_list[nr] = 0;
		return gen.list[nr];
	}
	return 0;
}