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


#include "search.h"
#include "trans.h"
#include "syzygy.h"
#include "eval.h"
#include "misc.h"
#include "magic.h"
#include "bit.h"
#include "zobrist.h"
#include "uci.h"
#include "main.h"

void verify_expr(const bool& condition, const char* expression, const char* file, unsigned long line)
{
    // if the expression cannot be verified, the output with information about the failed expression
    // is redirected to a log file after the UCI command 'setoption Log value true'

    if (!(!condition))
        return;
    std::cout << "verification '" << expression << "' failed in file " << file << " line " << line << std::endl;
    abort();
}

int main()
{
	std::cout << "Monolith " << uci::version_number << std::endl;

	// initializing everything before entering the communication loop

	bit::init_masks();
	zobrist::init_keys();
	trans::create(uci::hash_size);
	magic::init_table();
	filesystem::init_path();
	eval::mirror_tables();
	search::init_tables();
	syzygy::init_tb(uci::syzygy.path);

	uci::loop();
	return 0;
}
