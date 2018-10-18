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


#include "eval.h"
#include "zobrist.h"
#include "syzygy.h"
#include "stream.h"
#include "attack.h"
#include "magic.h"
#include "uci.h"
#include "main.h"

int main(int, char* argv[])
{
	std::cout << "Monolith " << uci::version_number << std::endl;

	// initializing

	filestream::set_path(argv);
	filestream::open();
	uci::open_book();
	magic::index_table();
	attack::fill_tables();
	zobrist::init_keys();
	syzygy::init_tablebases(uci::syzygy.path);
	eval::fill_tables();

	// starting the UCI communication protocol loop

	uci::loop();
	return 0;
}