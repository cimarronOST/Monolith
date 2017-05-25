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


#pragma once

#include "movegen.h"
#include "position.h"
#include "main.h"

class sort
{
private:
	int score_list[lim::movegen]{ };
	int move_cnt;

	void sort_capt(pos &board, movegen &gen);

	int mvv_lva(pos &board, uint16 move) const;
	int mvv_lva_promo(pos &board, uint16 move) const;

public:
	sort() { };
	sort(pos &board, movegen &gen)
	{
		sort_qsearch(board, gen);
	}
	sort(pos &board, movegen &gen, uint16 *best_move, int history[][6][64])
	{
		sort_root(board, gen, best_move, history);
	}
	sort(pos &board, movegen &gen, uint16 *best_move, int history[][6][64], uint16 killer[][2], int depth)
	{
		sort_main(board, gen, best_move, history, killer, depth);
	}

	void sort_main(pos &board, movegen &gen, uint16 *best_move, int history[][6][64], uint16 killer[][2], int depth);
	void sort_root(pos &board, movegen &gen, uint16 *best_move, int history[][6][64]);
	void sort_qsearch(pos &board, movegen &gen);
	
	uint16 next(movegen &gen);
};