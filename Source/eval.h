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


#pragma once

#include "board.h"
#include "types.h"
#include "main.h"

// evaluating a position

class kingpawn_hash;

namespace eval
{
	using eval_score  = std::array<std::array<int, 2>, 2>;
	using attack_list = std::array<std::array<bit64, 6>, 2>;
	using all_attacks = std::array<bit64, 2>;
	using piece_count = std::array<std::array<int, 5>, 2>;

	void mirror_tables();

	// evaluation interface functions

	score static_eval(const board& pos, kingpawn_hash& hash);

	static_assert(white == 0 && black == 1);
	static_assert(mg    == 0 && eg    == 1);

	// material weights

	extern std::array<std::array<int, 6>, 2> piece_value;
	extern std::array<int, 6> phase_value;
	extern std::array<int, 6> complexity;

	// piece weights

	extern std::array<int, 2> bishop_pair;
	extern std::array<int, 2> bishop_color_pawns;
	extern std::array<int, 2> bishop_trapped;
	extern std::array<int, 2> knight_outpost;
	extern std::array<int, 4> knight_distance_kings;
	extern std::array<int, 2> major_on_7th;
	extern std::array<int, 2> rook_open_file;

	// piece threatening weights

	extern std::array<int, 2> threat_pawn;
	extern std::array<int, 2> threat_minor;
	extern std::array<int, 2> threat_rook;
	extern std::array<int, 2> threat_queen_by_minor;
	extern std::array<int, 2> threat_queen_by_rook;
	extern std::array<int, 2> threat_piece_by_pawn;

	// king threatening weights

	extern int weak_king_sq;
	extern std::array<int,  5> threat_king_by_check;
	extern std::array<int,  5> threat_king_weight;
	extern std::array<int, 60> threat_king;

	// pawn weights

	extern std::array<int, 2> isolated;
	extern std::array<int, 2> backward;
	extern std::array<std::array<std::array<int, 8>, 2>, 2> connected;
	extern std::array<int, 2> king_distance_cl;
	extern std::array<int, 2> king_distance_cl_x;
	extern std::array<std::array<int, 8>, 2> passed_rank;
	extern std::array<std::array<int, 8>, 2> shield_rank;
	extern std::array<std::array<int, 8>, 2> storm_rank;

	// mobility weights

	extern std::array<std::array<int,  9>, 2> knight_mobility;
	extern std::array<std::array<int, 14>, 2> bishop_mobility;
	extern std::array<std::array<int, 15>, 2> rook_mobility;
	extern std::array<std::array<int, 28>, 2> queen_mobility;

	// piece square tables

	extern std::array<std::array<std::array<int, 64>, 2>, 2> pawn_psq;
	extern std::array<std::array<std::array<int, 64>, 2>, 2> knight_psq;
	extern std::array<std::array<std::array<int, 64>, 2>, 2> bishop_psq;
	extern std::array<std::array<std::array<int, 64>, 2>, 2> rook_psq;
	extern std::array<std::array<std::array<int, 64>, 2>, 2> queen_psq;
	extern std::array<std::array<std::array<int, 64>, 2>, 2> king_psq;
}

// managing the king-pawn hash table which speeds up the evaluation function

class kingpawn_hash
{
private:
	// size of 1 << 11 correlates to a fixed pawn-hash table of ~98 KB per thread

	constexpr static std::size_t size{ 1U << 11 };

public:
	enum table_memory
	{
		allocate,
		allocate_none
	};

	// king-pawn hash entry is 48 bytes

	struct hash
	{
		key64 key;
		std::array<bit64, 2> passed;
		std::array<bit64, 2> attack;
		std::array<std::array<int16, 2>, 2> score;
	};

	static_assert(sizeof(hash) == 48);

	// actual table

	std::vector<hash> table{};
	constexpr static key64 mask{ size - 1 };

	// creating the table

	kingpawn_hash(table_memory memory)
	{
		if (memory == allocate)
		{
			table.resize(size);
			for (auto& t : table) t = hash{};
		}
	}
};