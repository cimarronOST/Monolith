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

#include <string>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <array>
#include <chrono>
#include <vector>
#include <tuple>

#include "move.h"
#include "thread.h"
#include "trans.h"
#include "types.h"

// interface of the Universal Chess Interface (UCI) communication protocol

namespace uci
{
	// fixed parameters

	const std::string version_number{ "3" };
	const std::string startpos{ "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1" };

	// synchronizing main searching thread with communication thread

	inline std::mutex mutex{};
	inline std::condition_variable cv{};
	inline std::atomic<bool> stop{ true };
	inline std::atomic<bool> infinite{ false };

	// alterable parameters

	inline bool chess960{ false };
	inline bool log{ false };
	inline bool ponder{ false };

	inline int thread_cnt{ 1 };
	inline int mv_cnt{};
	inline int mv_offset{};
	inline std::array<key64, 256> game_hash{};

	inline std::size_t multipv{ 1 };
	inline std::size_t hash_size{ 128 };
	inline milliseconds overhead{};

	inline struct search_limit
	{
		std::vector<move> searchmoves;
		milliseconds movetime;
		int64 nodes;
		depth dt;
		depth mate;
		void set_infinite();
	} limit{};

	inline std::string syzygy_path{ "<empty>" };
	inline depth syzygy_dt{ 5 };

	// main transposition hash-table

	inline trans hash_table{};

	// communication loop

	void loop();

	// output of current search information

	void info_iteration(sthread& thread);
	void info_bound(sthread& thread, int pv_n, score sc, bound bd);
	void info_currmove(sthread& thread, int pv_n, move mv, int mv_n);
	void info_bestmove(std::tuple<move, move> mv);
}
