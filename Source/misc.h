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


#pragma once

#include <streambuf>
#include <fstream>

#include "types.h"
#include "main.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <direct.h>
#define fd_error INVALID_HANDLE_VALUE

using mem_map = HANDLE;
using FD = HANDLE;

#else
#include <unistd.h> 
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#define fd_error -1

using mem_map = std::size_t;
using FD = int;
#endif

// providing filesystem functionality

namespace filesystem
{
	// keeping track of the binary file location

	extern std::string path;
	void init_path();
}

// providing memory functionality

namespace memory
{
	// a collection of OS-dependent memory functions to use the Syzygy endgame tablebases
	// all credits go to Ronald de Man:
	// https://github.com/syzygy1/tb
	// https://github.com/syzygy1/Cfish

	uint64 size_tb(FD fd);
	FD open_tb(std::string name);
	void close_tb(FD fd);

	void*  map(FD fd, mem_map& map);
	void unmap(void* data, mem_map& map);

	// pre-loading data from memory into cache

	void prefetch(char* address);
}

// providing logging functionality

struct syncbuf : public std::streambuf
{
	// overriding the standard stream-buffer
	// idea from the Stockfish-team:
	// https://github.com/official-stockfish/Stockfish

	std::streambuf* logbuf{}, * stdbuf{};

	syncbuf(std::streambuf* sb1, std::streambuf* sb2) : logbuf{ sb1 }, stdbuf{ sb2 } {}

	int sync() override;
	int overflow(int c) override;
	int underflow() override;
	int uflow() override;
};

class synclog
{
	// synchronizing std::cout & std::cin with the logging file-stream through the overridden stream-buffer

private:
	std::ofstream log{};

	syncbuf outbuf{ log.rdbuf(), std::cout.rdbuf() };
	syncbuf  inbuf{ log.rdbuf(), std::cin.rdbuf() };

public:
	void start();
	void stop();
};
