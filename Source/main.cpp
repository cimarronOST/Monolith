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


#include "engine.h"
#include "uci.h"
#include "main.h"

namespace
{
	static_assert(sizeof(uint8)  == 1, "char");
	static_assert(sizeof(uint16) == 2, "short");
	static_assert(sizeof(uint32) == 4, "int");
	static_assert(sizeof(uint64) == 8, "long long");
}

int main(int argc, char* argv[])
{
	std::cout << "Monolith " << uci::version_number << std::endl;

	engine::init_path(argv);
	engine::init_book();
	engine::init_magic();
	engine::init_attack();
	engine::init_zobrist();
	engine::init_eval();

	uci::loop();
	return 0;
}