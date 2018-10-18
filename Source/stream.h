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


#pragma once

#include <streambuf>
#include <fstream>

#include "main.h"

// overriding the standard stream-buffer

struct syncbuf : std::streambuf
{
	std::streambuf *to_file, *to_std;
	syncbuf(std::streambuf *sb1, std::streambuf *sb2) :
		to_file(sb1), to_std(sb2) {}

	int overflow(int c) override
	{
		if (!traits_type::eq_int_type(traits_type::eof(), c))
			return to_file->sputc(c), to_std->sputc(c);
		else
			return traits_type::not_eof(c);
	}

	int sync() override
	{
		return to_file->pubsync(), to_std->pubsync();
	}

};

// making sure a log-file is written if the tuning flag is set

#if defined(TUNE)
  #define LOG
#endif

// directing output to a log-file

#if defined(LOG)
  #define sync sync_log
#else
  #define sync std
#endif

// synchronizing a file-stream object with the standard stream output

namespace sync_log
{
	extern std::ostream cout;
}

// manipulating the file path

namespace filestream
{
	extern std::string fullpath;

	void set_path(char *argv[]);
	bool open();
}