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


#include "misc.h"

// establishing the filesystem's path

std::string filesystem::path{};

void filesystem::init_path([[maybe_unused]] char* argv[])
{
	// establishing the absolute path of the directory of the binary file
	// the Android NDK doesn't support std::filesystem yet, therefore the workaround is needed

#if defined(__ANDROID__)
	path.resize(1024, '\0');
	while (!getcwd(&path[0], path.size()))
	{
		if (errno != ERANGE)
			return;
		path.resize(path.size() * 2);
	}
	path.resize(path.find('\0'));
	path += "/";

#else
	path = std::filesystem::absolute(argv[0]).string();
	path.erase(path.find_last_of("\\") + 1, path.size());
#endif
}

// interacting with the memory system

FD memory::open_tb(std::string name)
{
	// opening Syzygy tablebase

#ifdef _WIN32
	return CreateFile(name.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_RANDOM_ACCESS, nullptr);
#else
	return open(name.c_str(), O_RDONLY);
#endif
}

void memory::close_tb(FD fd)
{
	// closing Syzygy tablebase

#ifdef _WIN32
	CloseHandle(fd);
#else
	close(fd);
#endif
}

uint64 memory::size_tb(FD fd)
{
	// determining the size of the Syzygy tablebase

#ifdef _WIN32
	DWORD size_high{};
	DWORD size_low{ GetFileSize(fd, &size_high) };
	return ((uint64)size_high << 32) | size_low;

#else
	struct stat statbuf;
	fstat(fd, &statbuf);
	return statbuf.st_size;
#endif
}

void* memory::map(FD fd, mem_map& map)
{
	// mapping the file into virtual memory for fast access

#ifdef _WIN32
	DWORD size_high{};
	DWORD size_low{ GetFileSize(fd, &size_high) };
	map = CreateFileMapping(fd, nullptr, PAGE_READONLY, size_high, size_low, nullptr);
	if (!map)
		return nullptr;
	return MapViewOfFile(map, FILE_MAP_READ, 0, 0, 0);

#else
	map = size_tb(fd);
	void* data = mmap(nullptr, map, PROT_READ, MAP_SHARED, fd, 0);
	madvise(data, map, MADV_RANDOM);
	return data == MAP_FAILED ? nullptr : data;
#endif
}

void memory::unmap(void* data, mem_map& map)
{
	// unmapping the file from virtual memory

	if (!data)
		return;

#if defined (_WIN32)
	UnmapViewOfFile(data);
	CloseHandle(map);
#else
	munmap(data, map);
#endif
}

void memory::prefetch(char* p)
{
	// pre-loading data into cache
	// this is used as a speed-up to fetch tt-entries as soon as possible
	// so that the CPU doesn't have to wait for the entry to be loaded from memory
	// the Intel compiler has to be prevented to optimizing this away

#if defined(__INTEL_COMPILER)
	__asm__("");
#endif

#if defined(__INTEL_COMPILER) || defined(_MSC_VER)
	_mm_prefetch(p, _MM_HINT_T0);
#else
	__builtin_prefetch(p);
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
		return logbuf->sputc(stdbuf->sputc(c));
	else
		return traits_type::not_eof(c);
}

int syncbuf::underflow()
{
	return stdbuf->sgetc();
}

int syncbuf::uflow()
{
	return logbuf->sputc(stdbuf->sbumpc());
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
