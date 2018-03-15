/*
  Monolith 0.4  Copyright (C) 2017 Jonas Mayr

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


#include "move.h"
#include "bit.h"
#include "attack.h"
#include "see.h"

namespace
{
	int gain[32]{ };
}

namespace
{
	int sum_gain(int depth)
	{
		// summing up the exchange balance

		while (--depth)
			gain[depth - 1] = -std::max(-gain[depth - 1], gain[depth]);
		return gain[0];
	}

	uint64 least_valuable(const board &pos, uint64 set, int &piece)
	{
		// selecting the least valuable attacker of the set

		for (auto &p : pos.pieces)
		{
			if (p & set)
			{
				auto sq1{ bit::scan(p & set) };
				piece = pos.piece_sq[sq1];
				return 1ULL << sq1;
			}
		}
		return 0ULL;
	}
}

int see::eval(const board &pos, uint32 move)
{
	// doing a static exchange evaluation of the square reached by <move>

	// credits go to Michael Hoffman:
	// https://chessprogramming.wikispaces.com/SEE+-+The+Swap+Algorithm

	auto sq1{ move::sq1(move) };
	auto sq2{ move::sq2(move) };
	auto attacker{ 1ULL << sq1 };

	auto occ{ pos.side[BOTH] };
	auto set{ attack::to_square(pos, sq2) };
	auto xray_block{ pos.pieces[PAWNS] | pos.pieces[ROOKS] | pos.pieces[BISHOPS] | pos.pieces[QUEENS] };

	int depth{ };
	int turn{ pos.turn };
	int piece{ pos.piece_sq[sq1] };

	assert(move::piece(move) == piece);
	assert(move::turn(move) == pos.turn);

	// looping through the exchange sequence

	gain[depth] = value[move::victim(move)];
	do
	{
		depth += 1;
		gain[depth] = value[piece] - gain[depth - 1];

		// pruning early if the exchange balance is decisive

		if (std::max(-gain[depth - 1], gain[depth]) < 0)
			break;

		turn ^= 1;
		set ^= attacker;
		occ ^= attacker;

		// adding uncovered x-ray attackers to the set & finding the least valuable attacker

		if (attacker & xray_block)
			set |= attack::add_xray(pos, sq2, occ);

		attacker = least_valuable(pos, set & pos.side[turn], piece);

	} while (attacker);

	return sum_gain(depth);
}