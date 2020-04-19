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

// including library files to be accessible by all other source files

#include <tuple>
#include <string>
#include <array>
#include <vector>
#include <chrono>
#include <limits>
#include <iostream>
#include <algorithm>

using std::chrono::milliseconds;

// defining a verification macro similar to assert() to facilitate debugging

void verify_expr(const bool& condition, const char* expression, const char* file, unsigned long line);

#if !defined(NDEBUG)
#define verify(expr) (verify_expr(expr, #expr, __FILE__, __LINE__))

#else
#define verify(expr) ((void)0)
#endif

// defining an additional verification macro for runtime-expensive expressions which have a big impact
// on the speed of the engine

#if defined(DEBUG_DEEP)
#define verify_deep(expr) (verify(expr))

#else
#define verify_deep(expr) ((void)0)
#endif