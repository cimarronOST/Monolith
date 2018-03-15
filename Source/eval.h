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


#pragma once

#include "pawn.h"
#include "position.h"
#include "main.h"

// evaluating a position statically

namespace eval
{
	void fill_tables();

	// main interface functions

	int   static_eval(const board &pos);
	void itemise_eval(const board &pos);

	// high level evaluation

	bool obvious_draw(const board &pos);
	int scale_draw(const board &pos, int score);
	int interpolate(const board &pos, int sum[][2][2]);
	void evaluate(const board &pos, int sum[][2][2]);

	// low level evaluation & weighting

	void pieces(const board &pos, int sum[][2][2], int col);

	void pawns(const board &pos, int sum[][2][2]);
	void pawn_base(const board &pos, pawn::hash &entry);
	void pawn_addendum(const board &pos, int sum[][2][2], pawn::hash &entry, int col);

	// material weights

	const int value[2][6]
	{
		{  90, 320, 330, 500, 1000, 0 },
		{ 100, 320, 350, 550, 1050, 0 }
	};

	const int phase_value[6]
	{ 0, 2, 2, 3, 7 };

	// piece weights

	const int bishop_pair[2]
	{ 15, 30 };
	const int knight_outpost[2]
	{ 5,  2 };

	const int major_on_7th[2]
	{ 10, 20 };
	const int rook_open_file[2]
	{ 25,  0 };

	// king safety weights
	// credits go to https://chessprogramming.wikispaces.com/King+Safety

	const int threat_value[5]
	{ 0, 2, 2, 3, 5 };
	const int king_threat[64]
	{
		  0,   0,   0,   2,   3,   5,   7,   9,
		 12,  15,  18,  22,  26,  30,  35,  39,
		 44,  50,  56,  62,  68,  75,  82,  85,
		 89,  97, 105, 113, 122, 131, 140, 150,
		169, 180, 191, 202, 213, 225, 237, 248,
		260, 272, 283, 295, 307, 319, 330, 342,
		354, 366, 377, 389, 401, 412, 424, 436,
		448, 459, 471, 483, 494, 500, 500, 500
	};

	// pawn weights

	const int isolated[2]
	{ 10, 20 };

	const int king_pp_distance[2]
	{ 20, 5 };
	static int passed_rank[2][8]
	{ { 0, 0, 0, 20, 50, 120, 190, 0 } };

	static int shield_rank[2][8]
	{ { 0, 0, 10, 20, 25, 25, 25, 0 } };
	static int storm_rank[2][8]
	{ { 0, 0, 0, 5, 10, 20, 0, 0 } };

	// mobility weights

	const int bishop_mob[2][14]
	{
		{ -15, -10, 0, 10, 18, 25, 30, 34, 37, 39, 40, 41, 42, 43 },
		{ -15, -10, 0, 10, 18, 25, 30, 34, 37, 39, 40, 41, 42, 43 }
	};
	const int rook_mob[2][15]
	{
		{  -8,  -5, -3, 0, 3,  5,  8, 10, 12, 13, 14, 15, 16, 17, 18 },
		{ -18, -10, -4, 2, 8, 14, 20, 26, 32, 37, 40, 42, 43, 44, 45 }
	};
	const int queen_mob[2][28]
	{
		{  -4, -3, -2, -1,  0,  1,  2,  3,  4,  5,  6,  6,  7,  7,
		    8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8 },
		{ -10, -8, -6, -4, -2,  0,  2,  4,  6,  8, 10, 12, 13, 14,
		   14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14 }
	};
	const int knight_mob[2][9]
	{
		{ -20, -15, -8, 0, 8, 12, 15, 17, 18 },
		{ -20, -15, -8, 0, 8, 12, 15, 17, 18 }
	};

	// piece square tables

	static int psqt[2][6][2][64]
	{
		{
			{
				{ // [WHITE][PAWNS][MG][SQ]
					 0,  0,  0,  0,  0,  0,  0,  0,
					50, 50, 50, 50, 50, 50, 50, 50,
					10, 10, 20, 30, 30, 20, 10, 10,
					 5,  5, 10, 25, 25, 10,  5,  5,
					 0,  0,  0, 20, 20,  0,  0,  0,
					 5, -5,-10,  0,  0,-10, -5,  5,
					 5, 10, 10,-25,-25, 10, 10,  5,
					 0,  0,  0,  0,  0,  0,  0,  0
				},
				{ // [WHITE][PAWNS][EG][SQ]
					 0,  0,  0,  0,  0,  0,  0,  0,
					30, 35, 45, 50, 50, 45, 35, 30,
					15, 20, 25, 30, 30, 25, 20, 15,
					 6,  8, 10, 15, 15, 10,  8,  6,
					 3,  5,  5, 10, 10,  5,  5,  3,
					 0,  3,  3,  6,  6,  3,  3,  0,
					-3,  0,  0,  3,  3,  0,  0, -3,
					 0,  0,  0,  0,  0,  0,  0,  0
				}
		    },
			{
				{ // [WHITE][KNIGHTS][MG][SQ]
					-40,-30,-20,-20,-20,-20,-30,-40,
					-20,-10,  0,  0,  0,  0,-10,-20,
					-10,  0, 10, 15, 15, 10,  0,-10,
					-10,  0, 15, 20, 20, 15,  0,-10,
					-10,  0, 15, 20, 20, 15,  0,-10,
					-10,  0, 10, 15, 15, 10,  0,-10,
					-20,-10,  0,  0,  0,  0,-10,-20,
					-40,-30,-20,-20,-20,-20,-30,-40
				},
				{ // [WHITE][KNIGHTS][EG][SQ]
					-40,-30,-20,-20,-20,-20,-30,-40,
					-20,-10,  0,  0,  0,  0,-10,-20,
					-10,  0, 10, 15, 15, 10,  0,-10,
					-10,  0, 15, 20, 20, 15,  0,-10,
					-10,  0, 15, 20, 20, 15,  0,-10,
					-10,  0, 10, 15, 15, 10,  0,-10,
					-20,-10,  0,  0,  0,  0,-10,-20,
					-40,-30,-20,-20,-20,-20,-30,-40
				}
			},
			{
				{ // [WHITE][BISHOPS][MG][SQ]
					-10,-10, -5, -5, -5, -5,-10,-10,
					-10,  0,  0,  0,  0,  0,  0,-10,
					 -5,  0,  5,  5,  5,  5,  0, -5,
					 -5,  3,  5, 10, 10,  5,  3, -5,
					 -5,  0,  5, 10, 10,  5,  0, -5,
					 -5,  5,  5,  5,  5,  5,  5, -5,
					-10,  3,  0,  0,  0,  0,  3,-10,
					-20,-20,-15,-15,-15,-15,-20,-20
				},
				{ // [WHITE][BISHOPS][EG][SQ]
					-20,-15,-10, -5, -5,-10,-15,-20,
					-15,  0,  0,  0,  0,  0,  0,-15,
					-10,  0,  5,  5,  5,  5,  0,-10,
					 -5,  0,  5, 10, 10,  5,  0, -5,
					 -5,  0,  5, 10, 10,  5,  0, -5,
					-10,  0,  5,  5,  5,  5,  0,-10,
					-15,  0,  0,  0,  0,  0,  0,-15,
					-20,-15,-10, -5, -5,-10,-15,-20
				}
			},
			{
				{ // [WHITE][ROOKS][MG][SQ]
					-8, -5,  0,  5,  5,  0, -5, -8,
					-8, -5,  0,  5,  5,  0, -5, -8,
					-8, -5,  0,  5,  5,  0, -5, -8,
					-8, -5,  0,  5,  5,  0, -5, -8,
					-8, -5,  0,  5,  5,  0, -5, -8,
					-8, -5,  0,  5,  5,  0, -5, -8,
					-8, -5,  0,  5,  5,  0, -5, -8,
					-8, -5,  0,  5,  5,  0, -5, -8
				},
				{ // [WHITE][ROOKS][EG][SQ]
					 0,  0,  0,  0,  0,  0,  0,  0,
					 0,  0,  0,  0,  0,  0,  0,  0,
					 0,  0,  0,  0,  0,  0,  0,  0,
					 0,  0,  0,  0,  0,  0,  0,  0,
					 0,  0,  0,  0,  0,  0,  0,  0,
					 0,  0,  0,  0,  0,  0,  0,  0,
					 0,  0,  0,  0,  0,  0,  0,  0,
					 0,  0,  0,  0,  0,  0,  0,  0,
				}
			},
			{
				{ // [WHITE][QUEENS][MG][SQ]
					-15,-10,-10, -5, -5,-10,-10,-15,
					-10,  0,  0,  0,  0,  0,  0,-10,
					-10,  0,  5,  5,  5,  5,  0,-10,
					 -5,  0,  5,  5,  5,  5,  0, -5,
					 -5,  0,  5,  5,  5,  5,  0, -5,
					-10,  0,  5,  5,  5,  5,  0,-10,
					-10,  0,  0,  0,  0,  0,  0,-10,
					-15,-10,-10, -5, -5,-10,-10,-15
				},
				{ // [WHITE][QUEENS][EG][SQ]
					-5, -5, -5, -5, -5, -5, -5, -5,
					-5,  0,  0,  0,  0,  0,  0, -5,
					-5,  0,  5,  5,  5,  5,  0, -5,
					-5,  0,  5,  8,  8,  5,  0, -5,
					-5,  0,  5,  8,  8,  5,  0, -5,
					-5,  0,  5,  5,  5,  5,  0, -5,
					-5,  0,  0,  0,  0,  0,  0, -5,
					-5, -5, -5, -5, -5, -5, -5, -5
				}
			},
			{
				{ // [WHITE][KINGS][MG][SQ]
					-20,-10,-30,-50,-50,-30,-10,-20,
					-10,  0,-20,-40,-40,-20,  0,-10,
					 -5,  5,-15,-35,-35,-15,  5, -5,
					  0, 10,-10,-30,-30,-10, 10,  0,
					 10, 20,  0,-20,-20,  0, 20, 10,
					 15, 25,  5,-15,-15,  5, 25, 15,
					 30, 40, 20,  0,  0, 20, 40, 30,
					 35, 45, 25,  5,  5, 25, 45, 35
				},
				{  // [WHITE][KINGS][EG][SQ]
					-70,-50,-40,-30,-30,-40,-50,-70,
					-50,-20,-10, -5, -5,-10,-20,-50,
					-40,-10,  0, 20, 20,  0,-10,-40,
					-30, -5, 20, 30, 30, 20, -5,-30,
					-30, -5, 20, 30, 30, 20, -5,-30,
					-40,-10,  0, 20, 20,  0,-10,-40,
					-50,-20,-10, -5, -5,-10,-20,-50,
					-70,-50,-40,-30,-30,-40,-50,-70
				}
			}
        }
	};
};