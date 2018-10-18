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


#if defined(_WIN32) || defined(__MINGW32__)
  #define WINDOWS
#endif

#if defined(__linux__) && !defined(__CYGWIN__)
  #define LINUX
#endif

#if defined(WINDOWS)
  #include <stdlib.h>
#elif defined(LINUX)
  #include <unistd.h>
#endif

#include "stream.h"

// stream-buffer manipulation

namespace sync_log
{
	std::ofstream fout;
	syncbuf sbuf(sync_log::fout.rdbuf(), std::cout.rdbuf());
}

std::ostream sync_log::cout(&sbuf);

// log file path details

std::string filestream::fullpath;

void filestream::set_path(char *argv[])
{
	// retrieving the full path of the directory of the binary file
	// not working with Cygwin

	fullpath = argv[0];
	fullpath.erase(fullpath.find_last_of("\\") + 1, fullpath.size());

#if defined(WINDOWS)

	char path[255]{};
	static_cast<void>(_fullpath(path, argv[0], sizeof(path)));
	fullpath = path;
	fullpath.erase(fullpath.find_last_of("\\") + 1, fullpath.size());

#elif defined(LINUX)

	fullpath = get_current_dir_name();
	fullpath += "/";

#endif
}

bool filestream::open()
{
	// opening the log file if the LOG switch is set

#if !defined(LOG)

	if (sync_log::fout.is_open())
		sync_log::fout.close();
	return true;

#endif

	if (sync_log::fout.is_open())
		return true;
	else
	{
		// trying to open the logfile

		std::string file{ fullpath + "monolith_logfile.txt" };
		sync_log::fout.clear();
		sync_log::fout.open(file);

		if (sync_log::fout.is_open())
			return true;
		else
			return false;
	}
}