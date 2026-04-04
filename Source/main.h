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

#include <iostream>

// defining a verification macro similar to assert() to facilitate debugging

namespace
{
    [[maybe_unused]] void verify_expr(const bool& condition, const char* expr, const char* file, unsigned long line)
    {
        // if an expression cannot be verified, the output with information about the failed expression
        // can be redirected to a log file with the UCI command 'setoption Log value true'

        if (!(!condition))
            return;
        std::cout << "verification '" << expr << "' failed in file " << file << " line " << line << std::endl;
        abort();
    }
}

#if !defined(NDEBUG)
#define verify(expr) (verify_expr(expr, #expr, __FILE__, __LINE__))
#else
#define verify(expr) ((void)0)
#endif
