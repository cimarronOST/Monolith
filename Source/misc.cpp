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


#include <filesystem>
#include <streambuf>
#include <string>

#include "main.h"
#include "types.h"
#include "misc.h"

// establishing the filesystem's path

void filesystem::init_path(std::string argv)
{
	// establishing the directory of the executable

	path = std::filesystem::canonical(argv).string();
	auto last_sep{ path.find_last_of("\\/") };
	path.resize(last_sep + 1);
}

// interacting with the filesystem

datafile filesystem::open_file(std::string name, [[maybe_unused]] desired_access access)
{
	// opening file, used for Syzygy table-bases

#if defined(_WIN32)
	switch (access)
	{
	case READ:  return CreateFile(name.c_str(), GENERIC_READ,  0, nullptr, OPEN_EXISTING, FILE_FLAG_RANDOM_ACCESS, nullptr);
	case WRITE: return CreateFile(name.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_FLAG_RANDOM_ACCESS, nullptr);
	default: return FILE_ERROR;
	}
#else
	return open(name.c_str(), O_RDONLY);
#endif
}

void filesystem::close_file(datafile df)
{
	// closing files

#if defined(_WIN32)
	CloseHandle(df);
#else
	close(df);
#endif
}

uint64 filesystem::size_file(datafile df)
{
	// determining the size of the datafile

#if defined(_WIN32)
	DWORD size_high{};
	DWORD size_low{ GetFileSize(df, &size_high) };
	return ((uint64)size_high << 32) | size_low;

#else
	struct stat statbuf;
	fstat(df, &statbuf);
	return statbuf.st_size;
#endif
}

void* memory::map(datafile df, memorymap& map)
{
	// mapping the file into virtual memory for fast access

#if defined(_WIN32)
	DWORD size_high{};
	DWORD size_low{ GetFileSize(df, &size_high) };
	map = CreateFileMapping(df, nullptr, PAGE_READONLY, size_high, size_low, nullptr);
	if (!map)
		return nullptr;
	return MapViewOfFile(map, FILE_MAP_READ, 0, 0, 0);

#else
	map = filesystem::size_file(df);
	void* data{ mmap(nullptr, map, PROT_READ, MAP_SHARED, df, 0) };
	madvise(data, map, MADV_RANDOM);
	return data == MAP_FAILED ? nullptr : data;
#endif
}

void memory::unmap(void* data, memorymap& map)
{
	// unmapping the file from virtual memory

	if (!data)
		return;

#if defined(_WIN32)
	UnmapViewOfFile(data);
	CloseHandle(map);
#else
	munmap(data, map);
#endif
}

void memory::prefetch(char* address)
{
	// pre-loading data into cache
	// this is used as a speed-up to fetch tt-entries as soon as possible
	// so that the CPU doesn't have to wait for the entry to be loaded from memory
	// the Intel compiler has to be prevented to optimizing this away

#if defined(__INTEL_COMPILER)
	__asm__("");
#endif
#if defined(__INTEL_COMPILER) || defined(_MSC_VER)
	_mm_prefetch(address, _MM_HINT_T0);
#else
	__builtin_prefetch(address);
#endif
}

// overriding stream-buffer functions to enable logging

int syncbuf::sync()
{
	return logbuf->pubsync(), stdbuf->pubsync();
}

int syncbuf::overflow(int c)
{
	if (!traits_type::eq_int_type(traits_type::eof(), c))
		return logbuf->sputc((char)stdbuf->sputc((char)c));
	else
		return traits_type::not_eof(c);
}

int syncbuf::underflow()
{
	return stdbuf->sgetc();
}

int syncbuf::uflow()
{
	return logbuf->sputc((char)stdbuf->sbumpc());
}

void synclog::start()
{
	// starting to log I/O-stream
	// trying to open the logfile first

	if (log.is_open())
		log.close();

	std::string path{ filesystem::path + "monolith_log.txt" };
	log.clear();
	log.open(path);

	// synchronizing the I/O-streams with the log

	if (log.is_open())
	{
		std::cout.rdbuf(&outbuf);
		std::cin.rdbuf(&inbuf);
	}
}

void synclog::stop()
{
	// stopping to log the I/O-stream

	if (log.is_open())
		log.close();

	std::cout.rdbuf(outbuf.stdbuf);
	std::cin.rdbuf(inbuf.stdbuf);
}

// pseudo random number generation

void rand_64xor::new_seed(square sq)
{
	// getting a new seed for the PRNG depending on the square
	// this speeds up magic number generation significantly; idea from Marco Costalba:
	// http://www.talkchess.com/forum3/viewtopic.php?t=39298

	verify(type::sq(sq));
	seed = magic_seed[sq >> 2];
}

bit64 rand_64xor::rand64()
{
	// xor-shift pseudo random number generation
	// idea from George Marsaglia:
	// https://www.jstatsoft.org/article/view/v008i14

	seed ^= seed >> 12;
	seed ^= seed << 25;
	seed ^= seed >> 27;
	return  seed * 0x2545f4914f6cdd1dULL;
}

uint64 rand_64xor::sparse64()
{
	// creating a sparse magic number

	return rand64() & rand64() & rand64();
}

bit64 rand_64::rand64()
{
	// creating a Zobrist hash key

	return uniform(rand_gen);
}
