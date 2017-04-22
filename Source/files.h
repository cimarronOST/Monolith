/*
  Monolith 0.1  Copyright (C) 2017 Jonas Mayr

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

#include <streambuf>
#include <fstream>

#include "main.h"

namespace
{
	struct syncbuf : std::streambuf
	{
		std::streambuf *to_file, *to_cons;
		syncbuf(std::streambuf *sb1, std::streambuf *sb2) :
			to_file(sb1), to_cons(sb2) {}

		int overflow(int c) override
		{
			if (!traits_type::eq_int_type(traits_type::eof(), c))
				return to_file->sputc(c), to_cons->sputc(c);
			else
				return traits_type::not_eof(c);
		}
		int sync() override
		{
			return to_file->pubsync(), to_cons->pubsync();
		}
	};
}

class sync_log
{
	static syncbuf sbuf;

public:
	static std::ofstream fout;
	static std::ostream cout;
};

namespace files
{
	void set_path(string &path);
	string get_path();

	bool open();
};