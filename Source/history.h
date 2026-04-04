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


#pragma once

#include <array>
#include <limits>

#include "move.h"
#include "board.h"
#include "types.h"


// history tables collect statistics (histories) during the search
// they improve move ordering and correct board evaluation scores

class history
{
private:
	// updating the history tables

	void update_entry(int& entry, int weight);
	void update_corr(int& entry, int weight);
	void update_quiet(move mv, const sstack* ss, int cnt, int bonus, int malus);
	void update_capture(move mv, const sstack* ss, int cnt, int bonus, int malus);
	int  idx_corr(key64& key);

	// history tables to correct the board evaluation through pieces on the board

	static constexpr int corr_size{ 16384 };
	using corr_table = std::array<std::array<int, corr_size>, 2>;

private:
	corr_table corr_pawn{};
	corr_table corr_minor{};
	corr_table corr_major{};
	std::array<corr_table, 2> corr_nonpawn{};

	// history table to correct the board evaluation through continuation sequences

public:
	std::array<std::array<std::array<std::array<int, 64>, 6>, 65>, 6> corr_cont{};

	// history tables to improve move ordering and help pruning and late move reduction during search

	std::array<std::array<std::array<int, 64>, 64>, 2> main{};
	std::array<std::array<std::array<std::array<int, 6>, 64>, 6>, 2> capture{};
	std::array<std::array<std::array<std::array<std::array<int, 2>, 64>, 6>, 65>, 6> continuation{};

	// history weights

	static const constexpr uint32 max{ 0x20000000 };
	static_assert(max * 4 == std::numeric_limits<unsigned int>::max() / 2 + 1);

	inline static struct weight
	{
		int hist_base;
		int hist_gravity;
		int base_bonus;
		int base_malus;
		int corr_base;
		int corr_gravity;
		int corr_bonus;
		std::array<int, 9> corr_mult;
	} w;

	// public interface

	void  update(move mv, const sstack* ss, int quiet_cnt, int capture_cnt, depth dt);
	void  update_corr(board& pos, sstack* ss, score best_sc, score eval, depth dt);
	score correct_sc(board& pos, sstack* ss, score sc);
	void  clear();

	// probing the history tables during search

	struct sc
	{
		int main;
		int cont1;
		int cont2;
		int all() const;
		void get(const history& hist, move mv, const sstack* stack);
	};
};