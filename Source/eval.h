/*
  Monolith 1.0  Copyright (C) 2017-2018 Jonas Mayr

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

#include "pawn.h"
#include "position.h"
#include "main.h"

// evaluating a position statically

namespace eval
{
	void mirror_tables();
	void fill_tables();

	// evaluation interface functions

	int   static_eval(const board &pos, pawn &hash);
	void itemise_eval(const board &pos, pawn &hash);

	// material weights

	extern int piece_value[2][6];
	extern int phase_value[6];

	// piece weights

	extern int    bishop_pair[2];
	extern int knight_outpost[2];
	extern int   major_on_7th[2];
	extern int rook_open_file[2];

	// threat weights

	extern int  pawn_threat[2];
	extern int minor_threat[2];
	extern int queen_threat[2];
	extern int queen_threat_minor[2];
	extern int  king_threat[64];
	extern int  king_threat_weight[5];
	
	// pawn weights

	extern int  isolated[2];
	extern int  backward[2];
	extern int connected[2][2][8];
	extern int king_distance_friend[2];
	extern int king_distance_enemy[2];
	extern int passed_rank[2][8];
	extern int shield_rank[2][8];
	extern int  storm_rank[2][8];

	// mobility weights

	extern int knight_mobility[2][9];
	extern int bishop_mobility[2][14];
	extern int   rook_mobility[2][15];
	extern int  queen_mobility[2][28];

	// piece square tables

	extern int   pawn_psq[2][2][64];
	extern int knight_psq[2][2][64];
	extern int bishop_psq[2][2][64];
	extern int   rook_psq[2][2][64];
	extern int  queen_psq[2][2][64];
	extern int   king_psq[2][2][64];
}