/*
  Monolith 0.1  Copyright(C) 2017 Jonas Mayr

  This file is part of Monolith.

  Monolith is free software : you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Monolith is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with Monolith.If not, see <http://www.gnu.org/licenses/>.
*/


#include "bitboard.h"
#include "movegen.h"
#include "game.h"

int game::moves;
string game::game_str;

uint16 game::movelist[lim::period];
uint64 game::hashlist[lim::period];

namespace
{
	bool lone_bishop(pos &board)
	{
		return (board.pieces[BISHOPS] | board.pieces[KINGS]) == board.side[BOTH];
	}
	bool lone_knight(pos &board)
	{
		return (board.pieces[KNIGHTS] | board.pieces[KINGS]) == board.side[BOTH];
	}
	const uint64 sqs[]{ 0xaa55aa55aa55aa55, 0x55aa55aa55aa55aa };
}

void game::reset()
{
	moves = 0;
	game_str.clear();

	for (auto &m : movelist) m = 0;
	for (auto &h : hashlist) h = 0;
}
void game::save_move(pos &board, uint16 move)
{
	movelist[moves] = move;
	hashlist[moves] = board.key;
	moves += 1;
}

bool draw::verify(pos &board, uint64 list[])
{
	if (board.half_moves >= 4 && by_rep(board, list, board.moves))
		return true;
	else if (by_material(board))
		return true;
	else if (board.half_moves >= 50)
		return true;
	else
		return false;
}
bool draw::by_material(pos &board)
{
	if (lone_bishop(board) && (!(sqs[white] & board.pieces[BISHOPS]) || !(sqs[black] & board.pieces[BISHOPS])))
		return true;

	if (lone_knight(board) && bb::popcnt(board.pieces[KNIGHTS]) == 1)
		return true;

	return false;
}
bool draw::by_rep(pos &board, uint64 list[], int size)
{
	size -= 1;
	for (int i{ 4 }; i <= board.half_moves && i <= size; i += 2)
	{
		if (list[size - i] == list[size])
			return true;
	}

	return false;
}