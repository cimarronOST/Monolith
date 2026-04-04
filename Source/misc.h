/*
  Monolith Copyright (C) 2017-2026 Jonas Mayr

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
#include <random>
#include <string>

#include "types.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#define FILE_ERROR INVALID_HANDLE_VALUE

using memorymap = HANDLE;
using datafile = HANDLE;

#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#define FILE_ERROR -1

using memorymap = std::size_t;
using datafile = int;
#endif

// providing filesystem functionality

namespace filesystem
{
	// keeping track of the binary file location

	inline std::string path{};
	void init_path(std::string argv);

	enum desired_access { READ, WRITE };

	uint64 size_file(datafile df);
	datafile open_file(std::string name, desired_access access);
	void close_file(datafile df);
}

// providing memory functionality

namespace memory
{
	// memory-mapping functions

	void*  map(datafile df, memorymap& map);
	void unmap(void* data, memorymap& map);

	// pre-loading data from memory into cache

	void prefetch(char* address);
}

// providing logging functionality

struct syncbuf : public std::streambuf
{
	// overriding the standard stream-buffer

	std::streambuf *logbuf{}, *stdbuf{};

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

// pseudo random number generation

class rand_64xor
{
	// pseudo random number generation through xor-shift
	// used to generate "magic" index numbers

private:
	// using pre-calculated seeds to speed up magic number generation

	static constexpr std::array<bit64, 16> magic_seed
	{ {
		0x5dd4569, 0x33180c2, 0x1ab24ce, 0x4fc6fd8,
		0x559921d, 0x0db6850, 0x0c6e669, 0x4e47fcf,
		0x252b1fa, 0x4319b7f, 0x201818c, 0x3dd84f7,
		0x5ede0dc, 0x1321cc8, 0x2b9b062, 0x290b5b5
	} };

	bit64 seed{};
	bit64 rand64();

public:
	void new_seed(square sq);
	bit64 sparse64();
};

class rand_64
{
	// pseudo random number generation with standard library functions
	// used to generate Zobrist hash keys

private:
	std::mt19937_64 rand_gen;
	std::uniform_int_distribution<bit64> uniform{};

public:
	rand_64() : rand_gen(5489U) {}
	bit64 rand64();
};
