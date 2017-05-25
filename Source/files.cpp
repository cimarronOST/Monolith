/*
  Monolith 0.2  Copyright (C) 2017 Jonas Mayr

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


#include "files.h"

std::ofstream sync_log::fout;
syncbuf sync_log::sbuf(sync_log::fout.rdbuf(), std::cout.rdbuf());

std::ostream sync_log::cout(&sbuf);

namespace
{
	string fullpath;
}

void files::set_path(const string &path)
{
	fullpath = path;
}
string files::get_path()
{
	return fullpath;
}
bool files::open()
{

#ifndef LOG_ON

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
		string path{ get_path() };
		path.erase(path.find_last_of("\\") + 1, path.size());
		path += "searchlog.txt";

		sync_log::fout.clear();
		sync_log::fout.open(path);

		if (sync_log::fout.is_open())
			return true;
		else
			return false;
	}
}
