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

#include <vector>
#include <string>

// tuning evaluation parameters based on Peter Österlund's Texel Tuning method:
// https://www.chessprogramming.org/Texel%27s_Tuning_Method

namespace tune
{
#if !defined(NDEBUG)
	void evaluation(std::vector<std::string>& tuning_files, int thread_cnt);
#else
	inline void evaluation([[maybe_unused]] std::vector<std::string>& tuning_files, [[maybe_unused]] int thread_cnt) {}
#endif
}
