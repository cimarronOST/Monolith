/*
  Monolith 0.3  Copyright (C) 2017 Jonas Mayr

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

#include "logfile.h"

std::ofstream sync_log::fout;
syncbuf sync_log::sbuf(sync_log::fout.rdbuf(), std::cout.rdbuf());

std::ostream sync_log::cout(&sbuf);

namespace
{
	std::string fullpath;
	const std::string log_name{ "monolith logfile.txt" };
}

void log_file::set_path(char *argv[])
{
	// retrieving the full path of the directory of the binary file

#if defined(_WIN32)

	char path[255] = "";
	_fullpath(path, argv[0], sizeof(path));

	fullpath = path;
	fullpath.erase(fullpath.find_last_of("\\") + 1, fullpath.size());

#elif defined(__linux__)

	fullpath = get_current_dir_name();
	fullpath += "/";

#endif
}

std::string log_file::get_path()
{
	return fullpath;
}

bool log_file::open()
{

#ifndef LOG

	if (sync_log::fout.is_open())
		sync_log::fout.close();
	return true;

#endif

	if (sync_log::fout.is_open())
	{
		return true;
	}
	else
	{
		// trying to open the logfile

		std::string path{ get_path() + log_name };

		sync_log::fout.clear();
		sync_log::fout.open(path);

		if (sync_log::fout.is_open())
			return true;
		else
			return false;
	}
}
