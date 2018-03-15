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


#if defined(__linux__)
#include <unistd.h>
#endif

#include "stream.h"

// streambuffer manipulation

std::ofstream sync_log::fout;
syncbuf sync_log::sbuf(sync_log::fout.rdbuf(), std::cout.rdbuf());

std::ostream sync_log::cout(&sbuf);

// log file path details

std::string filestream::fullpath;
const std::string filestream::name{ "monolith logfile.txt" };

void filestream::set_path(char *argv[])
{
	// retrieving the full path of the directory of the binary file

#if defined(_WIN32)

	char path[255]{ };
	auto buf{ _fullpath(path, argv[0], sizeof(path)) };
	assert(buf != NULL);

	fullpath = path;
	fullpath.erase(fullpath.find_last_of("\\") + 1, fullpath.size());

#elif defined(__linux__)

	fullpath = get_current_dir_name();
	fullpath += "/";

#endif
}

bool filestream::open()
{
	// opening the log file, or keeping it close if the LOG switch is not set

#ifndef LOG

	if (sync_log::fout.is_open())
		sync_log::fout.close();
	return true;

#endif

	if (sync_log::fout.is_open())
		return true;
	else
	{
		// trying to open the logfile

		sync_log::fout.clear();
		sync_log::fout.open(fullpath + name);

		if (sync_log::fout.is_open())
			return true;
		else
			return false;
	}
}