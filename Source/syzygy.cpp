/*
  Monolith 1.0
  Copyright (C) 2011-2015 Ronald de Man
  Copyright (C) 2017-2018 Jonas Mayr

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


// all credits go to Ronald de Man for creating the awesome tablebases and providing the probing code:
// https://github.com/syzygy1/tb
// the probing code has been modified to conform with the engine
// currently a little-endian architecture (x86) is expected
// 32-bit is only supported for up to 5-piece tables

#if defined(_WIN32)
  #define WIN32_LEAN_AND_MEAN
  #define NOMINMAX
  #include <windows.h>
#else
  #include <unistd.h>
  #include <sys/stat.h>
  #include <sys/mman.h>
  #include <fcntl.h>
#endif

#include <mutex>
#include <atomic>
#include <cstdint>

#include "utilities.h"
#include "uci.h"
#include "stream.h"
#include "move.h"
#include "zobrist.h"
#include "movegen.h"
#include "bit.h"
#include "syzygy.h"

// defining constants & types

static_assert((sizeof(void*) == 8 && INTPTR_MAX == INT64_MAX)
	       || (sizeof(void*) == 4 && INTPTR_MAX == INT32_MAX), "x64 & x32");

template<int> struct uintx;
template<> struct uintx<4> { typedef uint32 type; };
template<> struct uintx<8> { typedef uint64 type; };
typedef uintx<sizeof(void*)>::type uint_base;

enum tb_limit
{
	TB_PIECES    = 6,
	TB_HASH_BITS = 10,
	DTZ_ENTRIES  = 64,

	TB_MAX_PIECE = 254,
	TB_MAX_PAWN  = 256,
	TB_MAX_HASH  = 5
};

enum tb_magic
{
	WDL_MAGIC = 0x5d23e871,
	DTZ_MAGIC = 0xa50c66d7
};

enum tb_piece_index
{
	TB_PAWN   = 1,
	TB_KNIGHT = 2,
	TB_BISHOP = 3,
	TB_ROOK   = 4,
	TB_QUEEN  = 5,
	TB_KING   = 6,

	TB_PAWN_WHITE = TB_PAWN,
	TB_PAWN_BLACK = TB_PAWN | 8
};

struct pairs_data
{
	int8 *index_table;
	uint16 *size_table;
	uint8 *data;
	uint16 *offset;
	uint8 *sym_lenght;
	uint8 *sym_pattern;
	int block_size;
	int index_bits;
	int min_length;
	uint_base base[1];
};

struct tb_entry
{
	int8 *data;
	uint64 key;
	uint64 mapping;
	std::atomic<uint8> ready;
	uint8 piece_count;
	uint8 symmetric;
	uint8 has_pawns;
};

struct tb_entry_piece : tb_entry
{
	uint8 enc_type;
	pairs_data *precomp[2];
	int factor[2][TB_PIECES];
	uint8 pieces[2][TB_PIECES];
	uint8 norm[2][TB_PIECES];
};

struct tb_entry_pawn : tb_entry
{
	uint8 pawns[2];
	struct
	{
		pairs_data *precomp[2];
		int factor[2][TB_PIECES];
		uint8 pieces[2][TB_PIECES];
		uint8 norm[2][TB_PIECES];
	} file[4];
};

struct dtz_entry_piece : tb_entry
{
	uint8 enc_type;
	pairs_data *precomp;
	int factor[TB_PIECES];
	uint8 pieces[TB_PIECES];
	uint8 norm[TB_PIECES];

	// flags are: accurate, mapped, side

	uint8 flags;
	uint16 map_index[4];
	uint8 *map;
};

struct dtz_entry_pawn : tb_entry
{
	uint8 pawns[2];
	struct
	{
		pairs_data *precomp;
		int factor[TB_PIECES];
		uint8 pieces[TB_PIECES];
		uint8 norm[TB_PIECES];
	} file[4];

	uint8 flags[4];
	uint16 map_index[4][4];
	uint8 *map;
};

struct tb_hash_entry
{
	uint64 key;
	tb_entry *ptr;
};

struct dtz_table_entry
{
	uint64 key1;
	uint64 key2;
	tb_entry *entry;
};

int syzygy::max_pieces{};
int syzygy::tablebases{};

namespace
{
	// various variables to be initialized before probing

	bool initialized{};
	struct tb_size { int pieces; int pawns; } tb_count{};

	std::string path_string{};
	std::vector<std::string> paths{};

	tb_entry_piece  tb_piece[TB_MAX_PIECE]{};
	tb_entry_pawn   tb_pawn[TB_MAX_PAWN]{};
	tb_hash_entry   tb_hash[1 << TB_HASH_BITS][TB_MAX_HASH]{};
	dtz_table_entry dtz_table[DTZ_ENTRIES]{};

	// making sure SMP works

	std::mutex mutex;

	// defining constants & index tables

	const std::string wdl_suffix{ ".rtbw" };
	const std::string dtz_suffix{ ".rtbz" };

	constexpr uint8 pa_flags[]{  8,   0,  0,   0,  4 };
	constexpr int wdl_to_map[]{  1,   3,  0,   2,  0 };
	constexpr int wdl_to_dtz[]{ -1, -101, 0, 101,  1 };
	constexpr int wdl_to_score[]
	{
		SCORE_TBMATE * -1,
		SCORE_BLESSED_LOSS,
		SCORE_DRAW,
		SCORE_CURSED_WIN,
		SCORE_TBMATE
	};
}

// tablebase core part

namespace tbcore
{
	constexpr uint8 diagonal[]
	{
		 0,  0,  0,  0,  0,  0,  0,  8,
		 0,  1,  0,  0,  0,  0,  9,  0,
		 0,  0,  2,  0,  0, 10,  0,  0,
		 0,  0,  0,  3, 11,  0,  0,  0,
		 0,  0,  0, 12,  4,  0,  0,  0,
		 0,  0, 13,  0,  0,  5,  0,  0,
		 0, 14,  0,  0,  0,  0,  6,  0,
		15,  0,  0,  0,  0,  0,  0,  7
	};

	constexpr int8 off_diagonal[]
	{
		0,-1,-1,-1,-1,-1,-1,-1,
		1, 0,-1,-1,-1,-1,-1,-1,
		1, 1, 0,-1,-1,-1,-1,-1,
		1, 1, 1, 0,-1,-1,-1,-1,
		1, 1, 1, 1, 0,-1,-1,-1,
		1, 1, 1, 1, 1, 0,-1,-1,
		1, 1, 1, 1, 1, 1, 0,-1,
		1, 1, 1, 1, 1, 1, 1, 0
	};

	constexpr uint8 flip_diagonal[]
	{
		0,  8, 16, 24, 32, 40, 48, 56,
		1,  9, 17, 25, 33, 41, 49, 57,
		2, 10, 18, 26, 34, 42, 50, 58,
		3, 11, 19, 27, 35, 43, 51, 59,
		4, 12, 20, 28, 36, 44, 52, 60,
		5, 13, 21, 29, 37, 45, 53, 61,
		6, 14, 22, 30, 38, 46, 54, 62,
		7, 15, 23, 31, 39, 47, 55, 63
	};

	constexpr uint8 triangle[]
	{
		6, 0, 1, 2, 2, 1, 0, 6,
		0, 7, 3, 4, 4, 3, 7, 0,
		1, 3, 8, 5, 5, 8, 3, 1,
		2, 4, 5, 9, 9, 5, 4, 2,
		2, 4, 5, 9, 9, 5, 4, 2,
		1, 3, 8, 5, 5, 8, 3, 1,
		0, 7, 3, 4, 4, 3, 7, 0,
		6, 0, 1, 2, 2, 1, 0, 6
	};

	constexpr uint8 lower[]
	{
		28,  0,  1,  2,  3,  4,  5,  6,
		 0, 29,  7,  8,  9, 10, 11, 12,
		 1,  7, 30, 13, 14, 15, 16, 17,
		 2,  8, 13, 31, 18, 19, 20, 21,
		 3,  9, 14, 18, 32, 22, 23, 24,
		 4, 10, 15, 19, 22, 33, 25, 26,
		 5, 11, 16, 20, 23, 25, 34, 27,
		 6, 12, 17, 21, 24, 26, 27, 35
	};

	constexpr uint8 flap[]
	{
		0,  0,  0,  0,  0,  0,  0, 0,
		0,  6, 12, 18, 18, 12,  6, 0,
		1,  7, 13, 19, 19, 13,  7, 1,
		2,  8, 14, 20, 20, 14,  8, 2,
		3,  9, 15, 21, 21, 15,  9, 3,
		4, 10, 16, 22, 22, 16, 10, 4,
		5, 11, 17, 23, 23, 17, 11, 5,
		0,  0,  0,  0,  0,  0,  0, 0
	};

	constexpr uint8 flap_inverse[]
	{
		 8, 16, 24, 32, 40, 48,
		 9, 17, 25, 33, 41, 49,
		10, 18, 26, 34, 42, 50,
		11, 19, 27, 35, 43, 51
	};

	constexpr uint8 pawn_twist[]
	{
		 0,  0,  0,  0,  0,  0,  0,  0,
		47, 35, 23, 11, 10, 22, 34, 46,
		45, 33, 21,  9,  8, 20, 32, 44,
		43, 31, 19,  7,  6, 18, 30, 42,
		41, 29, 17,  5,  4, 16, 28, 40,
		39, 27, 15,  3,  2, 14, 26, 38,
		37, 25, 13,  1,  0, 12, 24, 36,
		 0,  0,  0,  0,  0,  0,  0,  0
	};

	constexpr uint8 file_to_file[]{ 0, 1, 2, 3, 3, 2, 1, 0 };

	constexpr int16 kk_map[][64]
	{
		{
			 -1, -1, -1,  0,  1,  2,  3,  4,
			 -1, -1, -1,  5,  6,  7,  8,  9,
			 10, 11, 12, 13, 14, 15, 16, 17,
			 18, 19, 20, 21, 22, 23, 24, 25,
			 26, 27, 28, 29, 30, 31, 32, 33,
			 34, 35, 36, 37, 38, 39, 40, 41,
			 42, 43, 44, 45, 46, 47, 48, 49,
			 50, 51, 52, 53, 54, 55, 56, 57
		},
		{
			 58, -1, -1, -1, 59, 60, 61, 62,
			 63, -1, -1, -1, 64, 65, 66, 67,
			 68, 69, 70, 71, 72, 73, 74, 75,
			 76, 77, 78, 79, 80, 81, 82, 83,
			 84, 85, 86, 87, 88, 89, 90, 91,
			 92, 93, 94, 95, 96, 97, 98, 99,
			100,101,102,103,104,105,106,107,
			108,109,110,111,112,113,114,115
		},
		{
			116,117, -1, -1, -1,118,119,120,
			121,122, -1, -1, -1,123,124,125,
			126,127,128,129,130,131,132,133,
			134,135,136,137,138,139,140,141,
			142,143,144,145,146,147,148,149,
			150,151,152,153,154,155,156,157,
			158,159,160,161,162,163,164,165,
			166,167,168,169,170,171,172,173
		},
		{
			174, -1, -1, -1,175,176,177,178,
			179, -1, -1, -1,180,181,182,183,
			184, -1, -1, -1,185,186,187,188,
			189,190,191,192,193,194,195,196,
			197,198,199,200,201,202,203,204,
			205,206,207,208,209,210,211,212,
			213,214,215,216,217,218,219,220,
			221,222,223,224,225,226,227,228
		},
		{
			229,230, -1, -1, -1,231,232,233,
			234,235, -1, -1, -1,236,237,238,
			239,240, -1, -1, -1,241,242,243,
			244,245,246,247,248,249,250,251,
			252,253,254,255,256,257,258,259,
			260,261,262,263,264,265,266,267,
			268,269,270,271,272,273,274,275,
			276,277,278,279,280,281,282,283
		},
		{
			284,285,286,287,288,289,290,291,
			292,293, -1, -1, -1,294,295,296,
			297,298, -1, -1, -1,299,300,301,
			302,303, -1, -1, -1,304,305,306,
			307,308,309,310,311,312,313,314,
			315,316,317,318,319,320,321,322,
			323,324,325,326,327,328,329,330,
			331,332,333,334,335,336,337,338
		},
		{
			 -1, -1,339,340,341,342,343,344,
			 -1, -1,345,346,347,348,349,350,
			 -1, -1,441,351,352,353,354,355,
			 -1, -1, -1,442,356,357,358,359,
			 -1, -1, -1, -1,443,360,361,362,
			 -1, -1, -1, -1, -1,444,363,364,
			 -1, -1, -1, -1, -1, -1,445,365,
			 -1, -1, -1, -1, -1, -1, -1,446
		},
		{
			 -1, -1, -1,366,367,368,369,370,
			 -1, -1, -1,371,372,373,374,375,
			 -1, -1, -1,376,377,378,379,380,
			 -1, -1, -1,447,381,382,383,384,
			 -1, -1, -1, -1,448,385,386,387,
			 -1, -1, -1, -1, -1,449,388,389,
			 -1, -1, -1, -1, -1, -1,450,390,
			 -1, -1, -1, -1, -1, -1, -1,451
		},
		{
			452,391,392,393,394,395,396,397,
			 -1, -1, -1, -1,398,399,400,401,
			 -1, -1, -1, -1,402,403,404,405,
			 -1, -1, -1, -1,406,407,408,409,
			 -1, -1, -1, -1,453,410,411,412,
			 -1, -1, -1, -1, -1,454,413,414,
			 -1, -1, -1, -1, -1, -1,455,415,
			 -1, -1, -1, -1, -1, -1, -1,456
		},
		{
			457,416,417,418,419,420,421,422,
			 -1,458,423,424,425,426,427,428,
			 -1, -1, -1, -1, -1,429,430,431,
			 -1, -1, -1, -1, -1,432,433,434,
			 -1, -1, -1, -1, -1,435,436,437,
			 -1, -1, -1, -1, -1,459,438,439,
			 -1, -1, -1, -1, -1, -1,460,440,
			 -1, -1, -1, -1, -1, -1, -1,461
		}
	};

	int piv_factor[]{ 31332, 28056, 462 };
	int binomial[5][64]{};
	int pawn_index[5][24]{};
	int pawn_factor[5][4]{};

	int8 *map(std::string filename, uint64 &mapping)
	{
		// mapping the file into virtual memory for fast access

		assert(!paths.empty());

#if defined(_WIN32)
		HANDLE fd{ INVALID_HANDLE_VALUE };
		for (auto &path : paths)
		{
			std::string file{ path + "\\" + filename };
			fd = CreateFile(file.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
				FILE_ATTRIBUTE_NORMAL, nullptr);
			if (fd != INVALID_HANDLE_VALUE) break;
		}
		if (fd == INVALID_HANDLE_VALUE)
			return nullptr;

		DWORD size_high{};
		DWORD size_low{ GetFileSize(fd, &size_high) };
		HANDLE map{ CreateFileMapping(fd, nullptr, PAGE_READONLY, size_high, size_low, nullptr) };

		if (!map)
		{
			sync::cout << "info string warning: CreateFileMapping() failed, name = " << filename << std::endl;
			throw STOP_TABLEBASE;
		}
		mapping = (uint64)map;

		auto data{ (int8*)MapViewOfFile(map, FILE_MAP_READ, 0, 0, 0) };
		if (!data)
		{
			sync::cout << "info string warning: MapViewOfFile() failed, name = " << filename
				<< ", error = " << GetLastError() << std::endl;
			throw STOP_TABLEBASE;
		}
		CloseHandle(fd);
#else
		int fd{ -1 };
		for (auto &path : paths)
		{
			std::string file{ path + "/" + filename };
			fd = open(file.c_str(), O_RDONLY);
			if (fd != -1) break;
		}
		if (fd == -1)
			return nullptr;

		struct stat statbuf{};
		fstat(fd, &statbuf);
		mapping = statbuf.st_size;
		auto data{ (int8*)mmap(nullptr, statbuf.st_size, PROT_READ, MAP_SHARED, fd, 0) };
		if (data == (int8*)(-1))
		{
			sync::cout << "info string warning: mmap() failed, name = " << filename << std::endl;
			throw STOP_TABLEBASE;
		}
		close(fd);
#endif
		return data;
	}

	void unmap(int8 *data, uint64 &mapping)
	{
		// unmapping the file from virtual memory

#if defined (_WIN32)
		if (!data) return;
		UnmapViewOfFile(data);
		CloseHandle((HANDLE)mapping);
#else
		if (!data) return;
		munmap(data, mapping);
#endif
	}

	void add_to_hash(tb_entry *ptr, uint64 &key)
	{
		// adding a tb-entry to the hash

		auto &hash{ tb_hash[key >> (64 - TB_HASH_BITS)] };
		for (int i{}; i < TB_MAX_HASH; ++i)
		{
			if (!hash[i].ptr)
			{
				hash[i].key = key;
				hash[i].ptr = ptr;
				return;
			}
		}
		sync::cout << "info string warning: tablebase hash limit too low" << std::endl;
		throw STOP_TABLEBASE;
	}

	void init(std::vector<int> pieces)
	{
		// initializing the tablebase
		// starting with creating the piece-acronym of the position

		std::string acronym;
		for (auto p : pieces)
			acronym += "PNBRQK"[p];
		acronym.insert(acronym.find('K', 1), "v");

		// opening & closing the tablebase to test it

		assert(!paths.empty());

#if defined(_WIN32)
		HANDLE fd{ INVALID_HANDLE_VALUE };
		for (auto &path : paths)
		{
			std::string file{ path + "\\" + acronym + wdl_suffix };
			fd = CreateFile(file.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
				FILE_ATTRIBUTE_NORMAL, nullptr);
			if (fd != INVALID_HANDLE_VALUE) break;
		}
		if (fd == INVALID_HANDLE_VALUE) return;
		CloseHandle(fd);
#else
		int fd{ -1 };
		for (auto &path : paths)
		{
			std::string file{ path + "/" + acronym + wdl_suffix };
			fd = open(file.c_str(), O_RDONLY);
			if (fd != -1) break;
		}
		if (fd == -1) return;
		close(fd);
#endif

		int piece_list[16]{};
		int color{};
		for (auto &letter : acronym)
		{
			if (letter == 'v') { color = 0x8; continue; }
			for (int piece{ TB_PAWN }; piece <= TB_KING; ++piece)
				if (" PNBRQK"[piece] == letter)
					piece_list[piece | color] += 1;
		}

		auto key  { zobrist::material_key(piece_list, 0) };
		auto key_2{ zobrist::material_key(piece_list, 1) };

		tb_entry *entry
		{
			piece_list[TB_PAWN_WHITE] + piece_list[TB_PAWN_BLACK] == 0
			? (tb_entry*)&tb_piece[tb_count.pieces++]
			: (tb_entry*)&tb_pawn[tb_count.pawns++]
		};

		if (tb_count.pieces == TB_MAX_PIECE)
		{
			sync::cout << "info string warning: tablebase piece-limit too low" << std::endl;
			throw STOP_TABLEBASE;
		}
		if (tb_count.pawns == TB_MAX_PAWN)
		{
			sync::cout << "info string warning: tablebase pawn-limit too low" << std::endl;
			throw STOP_TABLEBASE;
		}

		entry->key = key;
		entry->ready = 0;
		entry->piece_count = 0;
		for (auto p : piece_list)
			entry->piece_count += p;
		if (entry->piece_count > syzygy::max_pieces)
			syzygy::max_pieces = entry->piece_count;

		entry->symmetric = (key == key_2);
		entry->has_pawns = (piece_list[TB_PAWN_WHITE] + piece_list[TB_PAWN_BLACK] > 0);

		if (entry->has_pawns)
		{
			auto e{ (tb_entry_pawn*)entry };
			e->pawns[WHITE] = piece_list[TB_PAWN_WHITE];
			e->pawns[BLACK] = piece_list[TB_PAWN_BLACK];

			if (piece_list[TB_PAWN_BLACK] > 0
				&& (piece_list[TB_PAWN_WHITE] == 0 || piece_list[TB_PAWN_BLACK] < piece_list[TB_PAWN_WHITE]))
			{
				e->pawns[WHITE] = piece_list[TB_PAWN_BLACK];
				e->pawns[BLACK] = piece_list[TB_PAWN_WHITE];
			}
		}
		else
		{
			int count{};
			for (auto p : piece_list)
				if (p == 1) count += 1;
			((tb_entry_piece*)entry)->enc_type = count >= 3 ? 0 : 2;
		}
		add_to_hash(entry, key);
		if (key_2 != key)
			add_to_hash(entry, key_2);
	}

	void init_indices()
	{
		// initializing indices
		// binomial[k-1][n] = bin(n, k)

		for (int i{}; i < 5; ++i)
		{
			for (int j{}; j < 64; ++j)
			{
				auto f{ j };
				int  l{ 1 };
				for (int k{ 1 }; k <= i; ++k)
				{
					f *= (j - k);
					l *= (k + 1);
				}
				binomial[i][j] = f / l;
			}
		}

		for (int i{}; i < 5; ++i)
		{
			int s{}, j{};
			for (; j < 6; ++j)
			{
				pawn_index[i][j] = s;
				s += (i == 0) ? 1 : binomial[i - 1][pawn_twist[flap_inverse[j]]];
			}
			pawn_factor[i][0] = s;
			s = 0;
			for (; j < 12; ++j)
			{
				pawn_index[i][j] = s;
				s += (i == 0) ? 1 : binomial[i - 1][pawn_twist[flap_inverse[j]]];
			}
			pawn_factor[i][1] = s;
			s = 0;
			for (; j < 18; ++j)
			{
				pawn_index[i][j] = s;
				s += (i == 0) ? 1 : binomial[i - 1][pawn_twist[flap_inverse[j]]];
			}
			pawn_factor[i][2] = s;
			s = 0;
			for (; j < 24; ++j)
			{
				pawn_index[i][j] = s;
				s += (i == 0) ? 1 : binomial[i - 1][pawn_twist[flap_inverse[j]]];
			}
			pawn_factor[i][3] = s;
		}
	}
	
	uint64 encode_piece(tb_entry_piece *entry, uint8 norm[], int pos[], int factor[])
	{
		// encoding piece

		if (pos[0] & 0x04)
		{
			for (int p{}; p < entry->piece_count; ++p)
				pos[p] ^= 0x07;
		}
		if (pos[0] & 0x20)
		{
			for (int p{}; p < entry->piece_count; ++p)
				pos[p] ^= 0x38;
		}
		for (int p{}; p < entry->piece_count; ++p)
		{
			if (off_diagonal[pos[p]])
			{
				if (p < (entry->enc_type == 0 ? 3 : 2) && off_diagonal[pos[p]] > 0)
				{
					for (p = 0; p < entry->piece_count; ++p)
						pos[p] = flip_diagonal[pos[p]];
				}
				break;
			}
		}

		uint64 index{};
		int i{}, j{};
		switch (entry->enc_type)
		{
		case 0:
			i = {  pos[1] > pos[0] };
			j = { (pos[2] > pos[0]) + (pos[2] > pos[1]) };

			if (off_diagonal[pos[0]])
				index = triangle[pos[0]] * 63 * 62 + (pos[1] - i) * 62 + (pos[2] - j);
			else if (off_diagonal[pos[1]])
				index = 6 * 63 * 62 + diagonal[pos[0]] * 28 * 62 + lower[pos[1]] * 62 + pos[2] - j;
			else if (off_diagonal[pos[2]])
				index = 6 * 63 * 62 + 4 * 28 * 62 + (diagonal[pos[0]]) * 7 * 28 + (diagonal[pos[1]] - i) * 28 + lower[pos[2]];
			else
				index = 6 * 63 * 62 + 4 * 28 * 62 + 4 * 7 * 28 + (diagonal[pos[0]] * 7 * 6) + (diagonal[pos[1]] - i) * 6
				      + (diagonal[pos[2]] - j);
			i = 3;
			break;

		default:
			index = kk_map[triangle[pos[0]]][pos[1]];
			i = 2;
			break;
		}
		index *= factor[0];

		while (i < entry->piece_count)
		{
			int t{ norm[i] };
			for (auto j{ i }; j < i + t; ++j)
			{
				for (auto k{ j + 1 }; k < i + t; ++k)
					if (pos[j] > pos[k]) std::swap(pos[j], pos[k]);
			}
			int s{};
			for (auto m{ i }; m < i + t; ++m)
			{
				int p{ pos[m] }, j{};
				for (int l{}; l < i; ++l)
					j += (p > pos[l]);
				s += binomial[m - i][p - j];
			}
			index += static_cast<uint64>(s) * static_cast<uint64>(factor[i]);
			i += t;
		}

		return index;
	}
	
	int pawn_file(tb_entry_pawn *entry, int pos[])
	{
		// determining file of leftmost pawn and sort pawns

		for (int i{ 1 }; i < entry->pawns[0]; ++i)
		{
			if (flap[pos[0]] > flap[pos[i]])
				std::swap(pos[0], pos[i]);
		}
		return file_to_file[index::file(pos[0])];
	}

	uint64 encode_pawn(tb_entry_pawn *entry, uint8 norm[], int pos[], int factor[])
	{
		// encoding pawns

		if (pos[0] & 0x04)
		{
			for (int p{}; p < entry->piece_count; ++p)
				pos[p] ^= 0x07;
		}
		for (int p{ 1 }; p < entry->pawns[0]; ++p)
		{
			for (auto j{ p + 1 }; j < entry->pawns[0]; ++j)
				if (pawn_twist[pos[p]] < pawn_twist[pos[j]])
					std::swap(pos[p], pos[j]);
		}

		auto t{ entry->pawns[0] - 1 };
		auto index{ static_cast<uint64>(pawn_index[t][flap[pos[0]]]) };
		for (auto i{ t }; i > 0; --i)
			index += binomial[t - i][pawn_twist[pos[i]]];
		index *= factor[0];

		// remaining pawns

		auto i{ entry->pawns[0] };
		t = i + entry->pawns[1];
		if (t > i)
		{
			for (auto j{ i }; j < t; ++j)
			{
				for (auto k{ j + 1 }; k < t; ++k)
					if (pos[j] > pos[k]) std::swap(pos[j], pos[k]);
			}
			int s{};
			for (auto m{ i }; m < t; ++m)
			{
				int p{ pos[m] }, j{};
				for (int k{}; k < i; ++k)
					j += (p > pos[k]);
				s += binomial[m - i][p - j - 8];
			}
			index += static_cast<uint64>(s) * static_cast<uint64>(factor[i]);
			i = t;
		}

		while (i < entry->piece_count)
		{
			t = norm[i];
			for (auto j{ i }; j < i + t; ++j)
			{
				for (auto k{ j + 1 }; k < i + t; ++k)
					if (pos[j] > pos[k]) std::swap(pos[j], pos[k]);
			}
			int s{};
			for (auto m{ i }; m < i + t; ++m)
			{
				int p{ pos[m] }, j{};
				for (int k{}; k < i; ++k)
					j += (p > pos[k]);
				s += binomial[m - i][p - j];
			}
			index += static_cast<uint64>(s) * static_cast<uint64>(factor[i]);
			i += t;
		}

		return index;
	}

	namespace fill
	{
		int subfactor(int k, int n)
		{
			// placing k like pieces on n squares

			int f{ n }, l{ 1 };
			for (int i{ 1 }; i < k; ++i)
			{
				f *= n - i;
				l *= i + 1;
			}
			return f / l;
		}

		uint64 factors_piece(int factor[], int count, int order, uint8 norm[], uint8 enc_type)
		{
			// calculating factors to assist indexing each individual piece-table

			int n{ 64 - norm[0] };
			uint64 f{ 1ULL };

			for (int i{ norm[0] }, k{}; i < count || k == order; ++k)
			{
				if (k == order)
				{
					factor[0] = static_cast<int>(f);
					f *= piv_factor[enc_type];
				}
				else
				{
					factor[i] = static_cast<int>(f);
					f *= subfactor(norm[i], n);
					n -= norm[i];
					i += norm[i];
				}
			}
			return f;
		}

		uint64 factors_pawn(int factor[], int num, int order, int order_2, uint8 norm[], int file)
		{
			// calculating factors to assist indexing each individual pawn-table

			int i{ norm[0] };
			if (order_2 < 0x0f)
				i += norm[i];
			int n{ 64 - i };
			uint64 f{ 1ULL };

			for (int k{}; i < num || k == order || k == order_2; ++k)
			{
				if (k == order)
				{
					factor[0] = static_cast<int>(f);
					f *= pawn_factor[norm[0] - 1][file];
				}
				else if (k == order_2)
				{
					factor[norm[0]] = static_cast<int>(f);
					f *= subfactor(norm[norm[0]], 48 - norm[0]);
				}
				else
				{
					factor[i] = static_cast<int>(f);
					f *= subfactor(norm[i], n);
					n -= norm[i];
					i += norm[i];
				}
			}
			return f;
		}

		void norm_piece(tb_entry_piece *entry, uint8 norm[], uint8 pieces[])
		{
			// filling the normalization-array for piece-tables

			for (int i{}; i < entry->piece_count; ++i)
				norm[i] = 0;

			switch (entry->enc_type)
			{
			case 0:
				norm[0] = 3;
				break;
			default:
				norm[0] = 2;
				break;
			}

			for (int i{ norm[0] }; i < entry->piece_count; i += norm[i])
			{
				for (auto j{ i }; j < entry->piece_count && pieces[j] == pieces[i]; ++j)
					norm[i] += 1;
			}
		}

		void norm_pawn(tb_entry_pawn *entry, uint8 norm[], uint8 pieces[])
		{
			// filling the normalization-array for pawn-tables

			for (int i{}; i < entry->piece_count; ++i)
				norm[i] = 0;
			norm[0] = entry->pawns[0];
			if (entry->pawns[1])
				norm[entry->pawns[0]] = entry->pawns[1];

			for (int i{ entry->pawns[0] + entry->pawns[1] }; i < entry->piece_count; i += norm[i])
			{
				for (auto j{ i }; j < entry->piece_count && pieces[j] == pieces[i]; ++j)
					norm[i] += 1;
			}
		}
	}

	void init_pieces_piece_wdl(tb_entry_piece *entry, uint8 data[], uint64 tb_size[])
	{
		// filling various arrays to order pieces for each wdl-table

		for (int i{}; i < entry->piece_count; ++i)
			entry->pieces[0][i] = data[i + 1] & 0x0f;
		int order{ data[0] & 0x0f };
		fill::norm_piece(entry, entry->norm[0], entry->pieces[0]);
		tb_size[0] = fill::factors_piece(entry->factor[0], entry->piece_count, order, entry->norm[0], entry->enc_type);

		for (int i{}; i < entry->piece_count; ++i)
			entry->pieces[1][i] = data[i + 1] >> 4;
		order = data[0] >> 4;
		fill::norm_piece(entry, entry->norm[1], entry->pieces[1]);
		tb_size[1] = fill::factors_piece(entry->factor[1], entry->piece_count, order, entry->norm[1], entry->enc_type);
	}

	void init_pieces_piece_dtz(dtz_entry_piece *entry, uint8 data[], uint64 tb_size[])
	{
		// filling various arrays to order pieces for each dtz-table

		for (int i{}; i < entry->piece_count; ++i)
			entry->pieces[i] = data[i + 1] & 0x0f;
		int order{ data[0] & 0x0f };
		fill::norm_piece((tb_entry_piece*)entry, entry->norm, entry->pieces);
		tb_size[0] = fill::factors_piece(entry->factor, entry->piece_count, order, entry->norm, entry->enc_type);
	}

	void init_pieces_pawn_wdl(tb_entry_pawn *entry, uint8 data[], uint64 tb_size[], int f)
	{
		// filling various arrays to order pieces for each pawn-wdl-table

		int j{ 1 + (entry->pawns[1] > 0) };
		int order  { data[0] & 0x0f };
		int order_2{ entry->pawns[1] ? (data[1] & 0x0f) : 0x0f };

		for (int i{}; i < entry->piece_count; ++i)
			entry->file[f].pieces[0][i] = data[i + j] & 0x0f;
		fill::norm_pawn(entry, entry->file[f].norm[0], entry->file[f].pieces[0]);
		tb_size[0] = fill::factors_pawn(entry->file[f].factor[0], entry->piece_count, order, order_2, entry->file[f].norm[0], f);

		order   = data[0] >> 4;
		order_2 = entry->pawns[1] ? (data[1] >> 4) : 0x0f;

		for (int i{}; i < entry->piece_count; ++i)
			entry->file[f].pieces[1][i] = data[i + j] >> 4;
		fill::norm_pawn(entry, entry ->file[f].norm[1], entry->file[f].pieces[1]);
		tb_size[1] = fill::factors_pawn(entry->file[f].factor[1], entry->piece_count, order, order_2, entry->file[f].norm[1], f);
	}

	void init_pieces_pawn_dtz(dtz_entry_pawn *entry, uint8 data[], uint64 tb_size[], int f)
	{
		// filling various arrays to order pieces for each pawn-dtz-table

		int j{ 1 + (entry->pawns[1] > 0) };
		int order  { data[0] & 0x0f };
		int order_2{ entry->pawns[1] ? (data[1] & 0x0f) : 0x0f };

		for (int i{}; i < entry->piece_count; ++i)
			entry->file[f].pieces[i] = data[i + j] & 0x0f;
		fill::norm_pawn((tb_entry_pawn*)entry, entry->file[f].norm, entry->file[f].pieces);
		tb_size[0] = fill::factors_pawn(entry->file[f].factor, entry->piece_count, order, order_2, entry->file[f].norm, f);
	}

	void symbol_lenght(pairs_data *d, int s, int8 tmp[])
	{
		// calculating the length of symbols for Huffmann-compression

		auto w{ *(int*)(d->sym_pattern + 3 * s) };
		auto s2{ (w >> 12) & 0x0fff };
		if (s2 == 0x0fff)
			d->sym_lenght[s] = 0;
		else
		{
			auto s1{ w & 0x0fff };
			if (!tmp[s1]) symbol_lenght(d, s1, tmp);
			if (!tmp[s2]) symbol_lenght(d, s2, tmp);
			d->sym_lenght[s] = d->sym_lenght[s1] + d->sym_lenght[s2] + 1;
		}
		tmp[s] = 1;
	}

	pairs_data *init_pairs(uint8 data[], uint64 tb_size, uint64 size[], uint8 **next, uint8 &flags, int wdl)
	{
		// initializing tables

		pairs_data *d{};
		flags = data[0];
		if (data[0] & 0x80)
		{
			d = (pairs_data*)malloc(sizeof(pairs_data));
			d->index_bits = 0;
			d->min_length = { wdl ? data[1] : 0 };
			*next = data + 2;
			size[0] = size[1] = size[2] = 0;
			return d;
		}

		int block_size{ data[1] };
		int index_bits{ data[2] };
		auto real_count_blocks{ *(uint32*)(&data[4]) };
		auto count_blocks{ real_count_blocks + *(uint8*)(&data[3]) };
		int max_length{ data[8] };
		int min_length{ data[9] };
		int h{ max_length - min_length + 1 };
		int count_sym{ *(uint16*)(&data[10 + 2 * h]) };
		d = (pairs_data*)malloc(sizeof(pairs_data) + (h - 1) * sizeof(uint_base) + count_sym);
		d->block_size = block_size;
		d->index_bits = index_bits;
		d->offset = (uint16*)(&data[10]);
		d->sym_lenght = (uint8*)d + sizeof(pairs_data) + (h - 1) * sizeof(uint_base);
		d->sym_pattern = &data[12 + 2 * h];
		d->min_length = min_length;
		*next = &data[12 + 2 * h + 3 * count_sym + (count_sym & 1)];

		auto count_indices{ (tb_size + (1ULL << index_bits) - 1) >> index_bits };
		size[0] =  6ULL * count_indices;
		size[1] =  2ULL * count_blocks;
		size[2] = (1ULL << block_size) * real_count_blocks;

		int8 temp[4096]{};
		for (int i{}; i < count_sym; ++i)
			temp[i] = 0;
		for (int i{}; i < count_sym; ++i)
		{
			if (!temp[i])
				symbol_lenght(d, i, temp);
		}

		d->base[h - 1] = 0;
		for (auto i{ h - 2 }; i >= 0; --i)
			d->base[i] = (d->base[i + 1] + d->offset[i] - d->offset[i + 1]) / 2;
		for (int i{}; i < h; ++i)
			d->base[i] <<= sizeof(uint_base) * 8 - (min_length + i);

		d->offset -= d->min_length;
		return d;
	}

	bool init_table_wdl(tb_entry *entry, std::string &acronym)
	{
		// initializing win-draw-loss-tablebase

		uint8  *next{};
		uint64 tb_size[8]{};
		uint64 size[8 * 3]{};
		uint8  flags{};

		// memory-mapping the file first

		try { entry->data = map(acronym + wdl_suffix, entry->mapping); }
		catch (exception_type &ex)
		{
			assert(ex == STOP_TABLEBASE);
			return false;
		}

		if (!entry->data)
		{
			sync::cout << "info string warning: could not find " << acronym << wdl_suffix << std::endl;
			return false;
		}

		auto data{ (uint8*)entry->data };
		if (((uint32*)data)[0] != WDL_MAGIC)
		{
			sync::cout << "info string warning: corrupted tablebases" << std::endl;
			unmap(entry->data, entry->mapping);
			entry->data = 0;
			return false;
		}

		int split{ data[4] & 0x01 };
		int files{ data[4] & 0x02 ? 4 : 1 };
		data += 5;

		if (!entry->has_pawns)
		{
			auto e{ (tb_entry_piece*)entry };
			init_pieces_piece_wdl(e, data, tb_size);
			data += e->piece_count + 1;
			data += ((uintptr_t)data) & 0x01;

			e->precomp[0] = init_pairs(data, tb_size[0], size, &next, flags, 1);
			data = next;
			if (split)
			{
				e->precomp[1] = init_pairs(data, tb_size[1], &size[3], &next, flags, 1);
				data = next;
			}
			else
				e->precomp[1] = nullptr;

			e->precomp[0]->index_table = (int8*)data;
			data += size[0];
			if (split)
			{
				e->precomp[1]->index_table = (int8*)data;
				data += size[3];
			}

			e->precomp[0]->size_table = (uint16*)data;
			data += size[1];
			if (split)
			{
				e->precomp[1]->size_table = (uint16*)data;
				data += size[4];
			}

			data = (uint8*)((((uintptr_t)data) + 0x3f) & ~0x3f);
			e->precomp[0]->data = data;
			data += size[2];
			if (split)
			{
				data = (uint8*)((((uintptr_t)data) + 0x3f) & ~0x3f);
				e->precomp[1]->data = data;
			}
		}
		else
		{
			auto e{ (tb_entry_pawn*)entry };
			int s{ 1 + (e->pawns[1] > 0) };
			for (int f{}; f < 4; ++f)
			{
				init_pieces_pawn_wdl(e, data, &tb_size[2 * f], f);
				data += e->piece_count + s;
			}
			data += ((uintptr_t)data) & 0x01;

			for (int f{}; f < files; ++f)
			{
				e->file[f].precomp[0] = init_pairs(data, tb_size[2 * f], &size[6 * f], &next, flags, 1);
				data = next;
				if (split)
				{
					e->file[f].precomp[1] = init_pairs(data, tb_size[2 * f + 1], &size[6 * f + 3], &next, flags, 1);
					data = next;
				}
				else
					e->file[f].precomp[1] = nullptr;
			}

			for (int f{}; f < files; ++f)
			{
				e->file[f].precomp[0]->index_table = (int8*)data;
				data += size[6 * f];
				if (split)
				{
					e->file[f].precomp[1]->index_table = (int8*)data;
					data += size[6 * f + 3];
				}
			}

			for (int f{}; f < files; ++f)
			{
				e->file[f].precomp[0]->size_table = (uint16*)data;
				data += size[6 * f + 1];
				if (split)
				{
					e->file[f].precomp[1]->size_table = (uint16*)data;
					data += size[6 * f + 4];
				}
			}

			for (int f{}; f < files; ++f)
			{
				data = (uint8*)((((uintptr_t)data) + 0x3f) & ~0x3f);
				e->file[f].precomp[0]->data = data;
				data += size[6 * f + 2];
				if (split)
				{
					data = (uint8*)((((uintptr_t)data) + 0x3f) & ~0x3f);
					e->file[f].precomp[1]->data = data;
					data += size[6 * f + 5];
				}
			}
		}
		return true;
	}

	bool init_table_dtz(tb_entry *entry)
	{
		// initializing distance-to-zero-tablebase

		auto data{ (uint8*)entry->data };
		uint8 *next{};
		uint64 tb_size[4]{};
		uint64 size[4 * 3]{};

		if (!data)
			return false;

		if (((uint32*)data)[0] != DTZ_MAGIC)
		{
			sync::cout << "info string warning: corrupted tablebases" << std::endl;
			return false;
		}

		int files{ data[4] & 0x02 ? 4 : 1 };
		data += 5;

		if (!entry->has_pawns)
		{
			auto e{ (dtz_entry_piece*)entry };
			init_pieces_piece_dtz(e, data, tb_size);
			data += e->piece_count + 1;
			data += ((uintptr_t)data) & 0x01;

			e->precomp = init_pairs(data, tb_size[0], size, &next, e->flags, 0);
			data = next;

			e->map = data;
			if (e->flags & 2)
			{
				for (int i{}; i < 4; ++i)
				{
					e->map_index[i] = static_cast<uint16>(data + 1 - e->map);
					data += 1 + data[0];
				}
				data += ((uintptr_t)data) & 0x01;
			}

			e->precomp->index_table = (int8*)data;
			data += size[0];

			e->precomp->size_table = (uint16*)data;
			data += size[1];

			data = (uint8*)((((uintptr_t)data) + 0x3f) & ~0x3f);
			e->precomp->data = data;
			data += size[2];
		}
		else
		{
			auto e{ (dtz_entry_pawn*)entry };
			int s{ 1 + (e->pawns[1] > 0) };
			for (int f{}; f < 4; ++f)
			{
				init_pieces_pawn_dtz(e, data, &tb_size[f], f);
				data += e->piece_count + s;
			}
			data += ((uintptr_t)data) & 0x01;

			for (int f{}; f < files; ++f)
			{
				e->file[f].precomp = init_pairs(data, tb_size[f], &size[3 * f], &next, e->flags[f], 0);
				data = next;
			}

			e->map = data;
			for (int f{}; f < files; ++f)
			{
				if (e->flags[f] & 2)
				{
					for (int i{}; i < 4; ++i)
					{
						e->map_index[f][i] = static_cast<uint16>(data + 1 - e->map);
						data += 1 + data[0];
					}
				}
			}
			data += ((uintptr_t)data) & 0x01;

			for (int f{}; f < files; ++f)
			{
				e->file[f].precomp->index_table = (int8*)data;
				data += size[3 * f];
			}

			for (int f{}; f < files; ++f)
			{
				e->file[f].precomp->size_table = (uint16*)data;
				data += size[3 * f + 1];
			}

			for (int f{}; f < files; ++f)
			{
				data = (uint8*)((((uintptr_t)data) + 0x3f) & ~0x3f);
				e->file[f].precomp->data = data;
				data += size[3 * f + 2];
			}
		}
		return true;
	}

	uint8 decompress_pairs(pairs_data *d, uint64 index)
	{
		// decompressing tables during probing

		if (!d->index_bits)
			return d->min_length;

		auto main_index{ static_cast<uint32>(index >> d->index_bits) };
		auto lit_index { static_cast<int>((index & ((1ULL << d->index_bits) - 1)) - (1ULL << (d->index_bits - 1))) };
		auto block     { *(uint32*)(d->index_table + 6 * main_index) };
		lit_index +=     *(uint16*)(d->index_table + 6 * main_index + 4);

		if (lit_index < 0)
		{
			do
			{
				lit_index += d->size_table[--block] + 1;
			} while (lit_index < 0);
		}
		else
		{
			while (lit_index > d->size_table[block])
				lit_index -= d->size_table[block++] + 1;
		}

		auto ptr{ (uint32*)(d->data + (block << d->block_size)) };

		int m{ d->min_length };
		uint16 *offset{ d->offset };
		uint_base *base{ d->base - m };
		uint_base symbol{};
		uint8 *sym_lenght{ d->sym_lenght };
		int bitcount{};

		if (sizeof(uint_base) == 8)
		{
			// code for 64-bit OS
			// bitcount is the number of "empty bits" in code

			auto code{ bit::byteswap64(*((uint64*)ptr)) };
			ptr += 2;
			bitcount = 0;
			for (;;)
			{
				auto l{ m };
				while (code < base[l]) ++l;
				symbol = offset[l] + ((code - base[l]) >> (64 - l));
				if (lit_index < (int)sym_lenght[symbol] + 1)
					break;
				lit_index    -= (int)sym_lenght[symbol] + 1;
				code <<= l;
				bitcount += l;
				if (bitcount >= 32)
				{
					bitcount -= 32;
					code |= static_cast<uint64>(bit::byteswap32(*ptr++)) << bitcount;
				}
			}
		}
		else
		{
			// code for 32-bit OS
			// bitcount is the number of bits in next

			assert(sizeof(uint_base) == 4);
			uint32 next{};
			auto code{ bit::byteswap32(*ptr++) };
			bitcount = 0;
			for (;;)
			{
				auto l{ m };
				while (code < base[l]) ++l;
				symbol = offset[l] + ((code - base[l]) >> (32 - l));
				if (lit_index < (int)sym_lenght[symbol] + 1)
					break;
				lit_index    -= (int)sym_lenght[symbol] + 1;
				code <<= l;
				if (bitcount < l)
				{
					if (bitcount)
					{
						code |= (next >> (32 - l));
						l -= bitcount;
					}
					next = bit::byteswap32(*ptr++);
					bitcount = 32;
				}
				code |= (next >> (32 - l));
				next <<= l;
				bitcount -= l;
			}
		}

		auto sym_pattern{ d->sym_pattern };
		while (sym_lenght[symbol] != 0)
		{
			auto w { *(int*)(sym_pattern + 3 * symbol) };
			auto s1{ w & 0x0fff };
			if (lit_index < (int)sym_lenght[s1] + 1)
				symbol = s1;
			else
			{
				lit_index -= (int)sym_lenght[s1] + 1;
				symbol = (w >> 12) & 0x0fff;
			}
		}

		return *(sym_pattern + 3 * symbol);
	}

	void load_dtz_table(std::string &acronym, uint64 key1, uint64 key2)
	{
		dtz_table[0] = dtz_table_entry{ key1, key2, nullptr };

		// finding corresponding WDL entry

		auto &hash{ tb_hash[key1 >> (64 - TB_HASH_BITS)] };
		int i{};
		for ( ; i < TB_MAX_HASH; ++i)
			if (hash[i].key == key1) break;
		if (i == TB_MAX_HASH)
			return;

		auto ptr{ hash[i].ptr };
		auto ptr_new{ (tb_entry*)malloc(ptr->has_pawns ? sizeof(dtz_entry_pawn) : sizeof(dtz_entry_piece)) };

		try
		{
			ptr_new->data = map(acronym + dtz_suffix, ptr_new->mapping);
			ptr_new->key = ptr->key;
			ptr_new->piece_count = ptr->piece_count;
			ptr_new->symmetric = ptr->symmetric;
			ptr_new->has_pawns = ptr->has_pawns;
		}
		catch (exception_type &ex)
		{
			assert(ex == STOP_TABLEBASE);
			free(ptr_new);
			return;
		}
		if (ptr_new->has_pawns)
		{
			auto entry{       (dtz_entry_pawn*)ptr_new };
			entry->pawns[0] = ((tb_entry_pawn*)ptr)->pawns[0];
			entry->pawns[1] = ((tb_entry_pawn*)ptr)->pawns[1];
		}
		else
		{
			auto entry{        (dtz_entry_piece*)ptr_new };
			entry->enc_type = ((dtz_entry_piece*)ptr)->enc_type;
		}
		if (!init_table_dtz(ptr_new))
			free(ptr_new);
		else
			dtz_table[0].entry = ptr_new;
	}

	void free_wdl_entry(tb_entry *entry)
	{
		unmap(entry->data, entry->mapping);
		if (!entry->has_pawns)
		{
			auto ptr{ (tb_entry_piece*)entry };
			free(ptr->precomp[0]);
			if  (ptr->precomp[1])
				free(ptr->precomp[1]);
		}
		else
		{
			auto ptr{ (tb_entry_pawn*)entry };
			for (int f{}; f < 4; ++f)
			{
				free(ptr->file[f].precomp[0]);
				if  (ptr->file[f].precomp[1])
					free(ptr->file[f].precomp[1]);
			}
		}
	}

	void free_dtz_entry(tb_entry *entry)
	{
		unmap(entry->data, entry->mapping);
		if (!entry->has_pawns)
		{
			auto ptr{ (dtz_entry_piece*)entry };
			free(ptr->precomp);
		}
		else
		{
			auto ptr{ (dtz_entry_pawn*)entry };
			for (int f{}; f < 4; ++f)
				free(ptr->file[f].precomp);
		}
		free(entry);
	}
}

// tablebase probing part

namespace syzygy
{
	std::string create_acronym(board &pos, int mirror)
	{
		// producing an acronym text string of the position, e.g. 'KQPvKRP'

		assert(mirror == WHITE || mirror == BLACK);

		std::string acronym;
		int color{ mirror ? BLACK : WHITE };
		while (true)
		{
			for (int p{ KINGS }; p >= PAWNS; --p)
				acronym += std::string(bit::popcnt(pos.pieces[p] & pos.side[color]), "PNBRQK"[p]);
			if (mirror != color) break;
			acronym += "v";
			color ^= 1;
		}
		return acronym;
	}

	int probe_wdl_table(board &pos, int &success)
	{
		// probing the win-draw-loss-tablebase
		// testing for KvK first

		auto key{ zobrist::material_key(pos, WHITE) };
		if (key == (zobrist::rand_key[(KINGS * 2 + WHITE) * 64] ^ zobrist::rand_key[(KINGS * 2 + BLACK) * 64]))
			return 0;

		auto &hash{ tb_hash[key >> (64 - TB_HASH_BITS)] };
		int i{};
		for ( ; i < TB_MAX_HASH; ++i)
			if (hash[i].key == key) break;
		if (i == TB_MAX_HASH)
		{
			success = 0;
			return 0;
		}

		auto ptr{ hash[i].ptr };
		if (!ptr->ready.load(std::memory_order_acquire))
		{
			std::unique_lock<std::mutex> lock(mutex);
			if (!ptr->ready.load(std::memory_order_relaxed))
			{
				auto acronym{ create_acronym(pos, ptr->key != key) };
				if (!tbcore::init_table_wdl(ptr, acronym))
				{
					hash[i].key = 0ULL;
					success = 0;
					return 0;
				}
				ptr->ready.store(1, std::memory_order_release);
			}
		}

		int bside{}, mirror{}, cmirror{};
		if (!ptr->symmetric)
		{
			if (key != ptr->key) {
				cmirror = 8;
				mirror = 0x38;
				bside = (pos.turn == WHITE);
			}
			else
			{
				cmirror = mirror = 0;
				bside = !(pos.turn == WHITE);
			}
		}
		else
		{
			cmirror = pos.turn == WHITE ? 0 : 8;
			mirror  = pos.turn == WHITE ? 0 : 0x38;
			bside = 0;
		}

		// saving the square for each piece
		// pieces of the same type are guaranteed to be consecutive

		int sq[TB_PIECES]{};
		uint8 result{};
		if (!ptr->has_pawns)
		{
			auto entry{ (tb_entry_piece*)ptr };
			auto pieces{ entry->pieces[bside] };
			for (i = 0; i < entry->piece_count; )
			{
				assert((pieces[i] & 0x07) - 1 >= PAWNS && (pieces[i] & 0x07) - 1 <= KINGS);
				auto b{ pos.side[(pieces[i] ^ cmirror) >> 3] & pos.pieces[(pieces[i] & 0x7) - 1] };
				while (b)
				{
					sq[i++] = square::flip(bit::scan(b));
					b &= b - 1;
				} 
			}
			auto index{ tbcore::encode_piece(entry, entry->norm[bside], sq, entry->factor[bside]) };
			result = tbcore::decompress_pairs(entry->precomp[bside], index);
		}
		else
		{
			auto entry{ (tb_entry_pawn*)ptr };
			int k{ entry->file[0].pieces[0][0] ^ cmirror };
			auto b{ pos.side[k >> 3] & pos.pieces[(k & 0x7) - 1] };
			i = 0;
			while (b)
			{
				sq[i++] = square::flip(bit::scan(b)) ^ mirror;
				b &= b - 1;
			}

			int f{ tbcore::pawn_file(entry, sq) };
			auto pieces{ entry->file[f].pieces[bside] };
			for ( ; i < entry->piece_count; )
			{
				b = pos.side[(pieces[i] ^ cmirror) >> 3] & pos.pieces[(pieces[i] & 0x7) - 1];
				while (b)
				{
					sq[i++] = square::flip(bit::scan(b)) ^ mirror;
					b &= b - 1;
				}
			}
			auto idx{ tbcore::encode_pawn(entry, entry->file[f].norm[bside], sq, entry->file[f].factor[bside]) };
			result = tbcore::decompress_pairs(entry->file[f].precomp[bside], idx);
		}

		return static_cast<int>(result) - 2;
	}

	int probe_dtz_table(board &pos, int wdl, int &success)
	{
		// probing the distance-to-zero-tablebase
		// the value of WDL must correspond to the WDL value of the position without en-passant rights

		int i{};
		auto key{ zobrist::material_key(pos, WHITE) };
		{
			// locking is necessary here because all threads probe the dtz-table at the root position

			std::unique_lock<std::mutex> lock(mutex);
			if (dtz_table[0].key1 != key && dtz_table[0].key2 != key)
			{
				for (i = 1; i < DTZ_ENTRIES; ++i)
					if (dtz_table[i].key1 == key || dtz_table[i].key2 == key) break;
				if (i < DTZ_ENTRIES)
				{
					auto table_entry{ dtz_table[i] };
					for (; i > 0; --i)
						dtz_table[i] = dtz_table[i - 1];
					dtz_table[0] = table_entry;
				}
				else
				{
					auto hash{ tb_hash[key >> (64 - TB_HASH_BITS)] };
					for (i = 0; i < TB_MAX_HASH; ++i)
						if (hash[i].key == key) break;
					if (i == TB_MAX_HASH)
					{
						success = 0;
						return 0;
					}
					int mirror{ (hash[i].ptr->key != key) };
					auto acronym{ create_acronym(pos, mirror) };

					if (dtz_table[DTZ_ENTRIES - 1].entry)
						tbcore::free_dtz_entry(dtz_table[DTZ_ENTRIES - 1].entry);
					for (i = DTZ_ENTRIES - 1; i > 0; --i)
						dtz_table[i] = dtz_table[i - 1];

					tbcore::load_dtz_table(acronym, zobrist::material_key(pos, mirror), zobrist::material_key(pos, !mirror));
				}
			}
		}

		auto ptr{ dtz_table[0].entry };
		if (!ptr)
		{
			success = 0;
			return 0;
		}

		int bside, mirror, cmirror;
		if (!ptr->symmetric)
		{
			if (key != ptr->key)
			{
				cmirror = 8;
				mirror = 0x38;
				bside = (pos.turn == WHITE);
			}
			else
			{
				cmirror = mirror = 0;
				bside = !(pos.turn == WHITE);
			}
		}
		else
		{
			cmirror = { pos.turn == WHITE ? 0 : 8 };
			mirror  = { pos.turn == WHITE ? 0 : 0x38 };
			bside = 0;
		}

		int sq[TB_PIECES]{};
		int result{};
		if (!ptr->has_pawns)
		{
			auto entry{ (dtz_entry_piece*)ptr };
			if ((entry->flags & 1) != bside && !entry->symmetric)
			{
				success = -1;
				return 0;
			}
			auto pieces{ entry->pieces };
			for (i = 0; i < entry->piece_count; )
			{
				auto b{ pos.side[(pieces[i] ^ cmirror) >> 3] & pos.pieces[(pieces[i] & 0x7) - 1] };
				while (b)
				{
					sq[i++] = square::flip(bit::scan(b));
					b &= b - 1;
				}
			}
			auto index{ tbcore::encode_piece((tb_entry_piece*)entry, entry->norm, sq, entry->factor) };
			result = tbcore::decompress_pairs(entry->precomp, index);

			if (entry->flags & 2)
				result = entry->map[entry->map_index[wdl_to_map[wdl + 2]] + result];

			if (!(entry->flags & pa_flags[wdl + 2]) || (wdl & 1))
				result *= 2;
		}
		else
		{
			auto entry{ (dtz_entry_pawn*)ptr };
			int k{ entry->file[0].pieces[0] ^ cmirror };
			auto b{ pos.side[k >> 3] & pos.pieces[(k & 0x7) - 1] };

			while (b)
			{
				sq[i++] = square::flip(bit::scan(b)) ^ mirror;
				b &= b - 1;
			}
			auto f{ tbcore::pawn_file((tb_entry_pawn*)entry, sq) };
			if ((entry->flags[f] & 1) != bside)
			{
				success = -1;
				return 0;
			}
			auto pieces{ entry->file[f].pieces };
			for (i = 0; i < entry->piece_count; )
			{
				b = pos.side[(pieces[i] ^ cmirror) >> 3] & pos.pieces[(pieces[i] & 0x7) - 1];
				while (b)
				{
					sq[i++] = square::flip(bit::scan(b)) ^ mirror;
					b &= b - 1;
				}
			}
			auto index{ tbcore::encode_pawn((tb_entry_pawn*)entry, entry->file[f].norm, sq, entry->file[f].factor) };
			result = tbcore::decompress_pairs(entry->file[f].precomp, index);

			if (entry->flags[f] & 2)
				result = entry->map[entry->map_index[f][wdl_to_map[wdl + 2]] + result];

			if (!(entry->flags[f] & pa_flags[wdl + 2]) || (wdl & 1))
				result *= 2;
		}

		return result;
	}

	int alphabeta(board& pos, int alpha, int beta, int &success)
	{
		// doing a small alpha-beta-search
		// generating all legal captures including (under)promotions

		int v{};
		gen list(pos, LEGAL);
		list.gen_tactical<PROMO_ALL>();

		board prev_pos(pos);
		for (int i{}; i < list.moves; ++i)
		{
			assert_exp(pos.pseudolegal(list.move[i]));
			if (!move::capture(list.move[i]))
				continue;

			pos.new_move(list.move[i]);
			assert_exp(pos.legal());

			v = -alphabeta(pos, -beta, -alpha, success);
			pos.revert(prev_pos);
			if (success == 0) return 0;
			if (v > alpha)
			{
				if (v >= beta)
					return v;
				alpha = v;
			}
		}

		v = probe_wdl_table(pos, success);
		return alpha >= v ? alpha : v;
	}
}

void syzygy::init_tablebases(std::string &path)
{
	// initializing all the tablebases that can be found in <path>

	if (!initialized)
	{
		tbcore::init_indices();
		initialized = true;
	}

	// cleaning up if path_string is set

	if (!path_string.empty())
	{
		path_string.clear();
		paths.clear();

		tb_entry *entry{};
		for (int i{}; i < tb_count.pieces; ++i)
		{
			entry = (tb_entry*)&tb_piece[i];
			tbcore::free_wdl_entry(entry);
		}
		for (int i{}; i < tb_count.pawns; ++i)
		{
			entry = (tb_entry*)&tb_pawn[i];
			tbcore::free_wdl_entry(entry);
		}
		for (auto &dtz : dtz_table)
		{
			if (dtz.entry) tbcore::free_dtz_entry(dtz.entry);
		}
	}

	// returning immediately if path is an empty string or equals "<empty>"

	path_string = path;
	if (path_string.empty() || path_string == "<empty>") return;

#if defined(_WIN32)
	auto seperation{ ';' };
#else
	auto seperation{ ':' };
#endif

	for (uint32 i{ 1 }, j{}; i < path_string.size(); ++i)
	{
		if (i + 1 == path_string.size() || path_string[i + 1] == seperation)
		{
			assert(i + 1 - j >= 0);
			paths.push_back(path_string.substr(j, i + 1 - j));
			j = i + 2;
		}
	}

	max_pieces = 0;
	tablebases = 0;
	tb_count   = tb_size{};

	for (auto &i   : tb_hash)   for (auto &hash : i) hash = tb_hash_entry{};
	for (auto &dtz : dtz_table) dtz.entry = nullptr;

	try
	{
		for (int p1{ PAWNS }; p1 <= QUEENS; ++p1)
		{
			tbcore::init({ KINGS, p1, KINGS });
			for (int p2{ PAWNS }; p2 <= p1; ++p2)
			{
				tbcore::init({ KINGS, p1, KINGS, p2 });
				tbcore::init({ KINGS, p1, p2, KINGS });

				for (int p3{ PAWNS }; p3 <= QUEENS; ++p3)
					tbcore::init({ KINGS, p1, p2, KINGS, p3 });

				for (int p3{ PAWNS }; p3 <= p2; ++p3)
				{
					tbcore::init({ KINGS, p1, p2, p3, KINGS });

					for (int p4{ p3 }; p4 <= QUEENS; ++p4)
						tbcore::init({ KINGS, p1, p2, p3, KINGS, p4 });
					for (int p4{ PAWNS }; p4 <= p3; ++p4)
						tbcore::init({ KINGS, p1, p2, p3, p4, KINGS });
				}
				for (int p3{ PAWNS }; p3 <= p1; ++p3)
					for (int p4{ PAWNS }; p4 <= (p1 == p3 ? p2 : p3); ++p4)
						tbcore::init({ KINGS, p1, p2, KINGS, p3, p4 });
			}
		}
		tablebases = tb_count.pieces + tb_count.pawns;
		sync::cout << "info string " << tablebases << " tablebases were found" << std::endl;
	}
	catch (exception_type &ex)
	{
		assert(ex == STOP_TABLEBASE);
		sync::cout << "info string warning: tablebases could not be initalized" << std::endl;
	}
}

int syzygy::probe_wdl(board &pos, int &success)
{
	// probing the win-draw-loss-table
	// if success != 0, the probe was successful
	// if success == 2, the position has a winning capture, or the position is a cursed win 
	// and has a cursed winning capture, or the position has an en-passant capture as only best move
	// this is used in probe_dtz()

	success = 1;

	// generating all subsequent legal captures including promotions

	gen list(pos, LEGAL);
	list.gen_tactical<PROMO_ALL>();
	int best_capture{ -3 };
	int best_ep{ -3 };

	// keeping track of the best capture & still better en-passant-capture of the subsequent moves

	board prev_pos(pos);
	for (int i{}; i < list.moves; ++i)
	{
		assert_exp(pos.pseudolegal(list.move[i]));
		if (!move::capture(list.move[i]))
			continue;

		pos.new_move(list.move[i]);
		assert_exp(pos.legal());

		auto v{ -alphabeta(pos, -2, -best_capture, success) };
		pos.revert(prev_pos);
		if (success == 0) return 0;
		if (v > best_capture)
		{
			if (v == 2)
			{
				success = 2;
				return 2;
			}
			if (move::flag(list.move[i]) != ENPASSANT)
				best_capture = v;
			else if (v > best_ep)
				best_ep = v;
		}
	}

	auto wdl{ probe_wdl_table(pos, success) };
	if (success == 0) return 0;

	// now max(wdl, best_cap) is the WDL value of the position without en-passant rights
	// if the position without en-passant rights is not stalemate or no en-passant captures exist, then the value of the position
	// is max(wdl, best_capture, best_ep). If the position without en-passant rights is stalemate and best_ep > -3, then the
	// value of the position is best_ep (and wdl == 0)

	if (best_ep > best_capture)
	{
		if (best_ep > wdl)
		{
			// en-passant capture (possibly cursed loss) is the best capture

			success = 2;
			return best_ep;
		}
		best_capture = best_ep;
	}

	// now max(wdl, best_capture) is the WDL value of the position unless
	// the position without en-passant rights is stalemate and best_ep > -3

	if (best_capture >= wdl)
	{
		success = 1 + (best_capture > 0);
		return best_capture;
	}

	// handling the stalemate case

	if (best_ep > -3 && wdl == 0)
	{
		// checking for stalemate in the position with en-passant captures

		int move_count{}, i{};
		for ( ; i < list.moves; ++i)
		{
			if (move::flag(list.move[i]) == ENPASSANT) continue;
			move_count += 1;
			break;
		}
		if (move_count == 0 && !pos.check())
		{
			list.gen_quiet();
			assert(list.count.promo == 0);
			assert(list.count.capture == i);
			for ( ; i < list.moves; ++i)
			{
				move_count += 1;
				break;
			}
		}
		if (move_count == 0)
		{
			// stalemate

			success = 2;
			return best_ep;
		}
	}

	// stalemate & en-passant not an issue, so wdl is correct

	return wdl;
}

int syzygy::probe_dtz(board &pos, int &success)
{
	// probing the distance-to-zero-table for a particular position
	// if success != 0, the probe was successful

	// the return value is from the point of view of the side to move:
	//         n < -100 : loss, but draw under 50-move rule
	// -100 <= n < -1   : loss in n ply (assuming 50-move counter == 0)
	//        -1        : position is mate
	//         0	    : draw
	//     1 < n <= 100 : win in n ply (assuming 50-move counter == 0)
	//   100 < n        : win, but draw under 50-move rule

	// the return value n can be off by 1: a return value -n can mean a loss in n+1 ply
	// and a return value +n can mean a win in n+1 ply. This cannot happen for tables with positions
	// exactly on the "edge" of the 50-move rule

	// this means that if dtz > 0 is returned, the position is certainly a win if dtz + 50-move-counter <= 99
	// care must be taken that the engine picks moves that preserve dtz + 50-move-counter <= 99

	// if n = 100 immediately after a capture or pawn move, then the position is also certainly a win,
	// and during the whole phase until the next capture or pawn move, the inequality to be preserved is
	// dtz + 50 - movecount <= 100

	// in short, if a move is available resulting in dtz + 50-move-counter <= 99,
	// then moves leading to dtz + 50-move-counter == 100 shall not be accepted

	auto wdl{ probe_wdl(pos, success) };
	if (success == 0) return 0;

	// draw

	if (wdl == 0) return 0;

	// checking for winning (cursed) capture or en-passant capture as only best move

	if (success == 2)
		return wdl_to_dtz[wdl + 2];

	// checking for a winning pawn move if the position is winning

	gen list(pos, LEGAL);
	if (wdl > 0)
	{
		// generating all legal quiet pawn moves including non-capturing promotions

		list.gen_all();
		board prev_pos(pos);
		for (int i{}; i < list.moves; ++i)
		{
			if (move::piece(list.move[i]) != PAWNS || move::capture(list.move[i]))
				continue;

			pos.new_move(list.move[i]);
			auto v{ -probe_wdl(pos, success) };
			pos.revert(prev_pos);
			if (success == 0) return 0;
			if (v == wdl)
				return wdl_to_dtz[wdl + 2];
		}
	}

	// the best move can not an en-passant capture now
	// in other words, the value of wdl corresponds to the wdl value of the position without en-passant rights
	// it is therefore safe to probe the dtz-table with the current value of wdl

	int dtz{ probe_dtz_table(pos, wdl, success) };
	if (success >= 0)
		return wdl_to_dtz[wdl + 2] + ((wdl > 0) ? dtz : -dtz);

	// success < 0 means that the dtz-table needs to be probed by the other side to move

	int best{};
	if (wdl > 0)
		best = std::numeric_limits<int32>::max();
	else
	{
		// if (cursed) loss, the worst case is a losing capture or pawn move as the "best" move,
		// leading to dtz of -1 or -101. In case of mate, this will cause -1 to be returned

		best = wdl_to_dtz[wdl + 2];
		assert(list.empty());
		list.gen_all();
	}

	board prev_pos(pos);
	for (int i{}; i < list.moves; ++i)
	{
		// pawn moves and captures can be skipped, because if wdl > 0 they were already caught,
		// and if wdl < 0 the initial value of best already takes account of them

		if (move::capture(list.move[i]) || move::piece(list.move[i]) == PAWNS)
			continue;

		pos.new_move(list.move[i]);
		auto v{ -probe_dtz(pos, success) };
		pos.revert(prev_pos);
		if (success == 0) return 0;
		if (wdl > 0)
		{
			if (v > 0 && v + 1 < best)
				best = v + 1;
		}
		else
		{
			if (v - 1 < best)
				best = v - 1;
		}
	}
	return best;
}

bool syzygy::probe_dtz_root(board &pos, rootpick &pick, uint64 repetition_hash[], int &tb_score)
{
	// using the distance-to-zero-tables to mark moves that preserve the win or draw
	// if the position is lost, but dtz is fairly high, only keeping moves that maximize dtz
	// a return value of false indicates that not all probes were successful and no moves were marked

	int success{};
	int dtz{ probe_dtz(pos, success) };
	if (!success) return false;

	// probing each move

	for (int i{}; i < pick.list.moves; ++i)
	{
		pos.new_move(pick.sort.root[i].move);
		int v{};

		if (pos.check() && dtz > 0)
		{
			gen list(pos, LEGAL);
			list.gen_all();
			if (list.empty())
				v = 1;
		}
		if (!v)
		{
			if (pos.half_count != 0)
			{
				v = -probe_dtz(pos, success);
				if      (v > 0) v += 1;
				else if (v < 0) v -= 1;
			}
			else
			{
				v = -probe_wdl(pos, success);
				v = wdl_to_dtz[v + 2];
			}
		}

		pos.revert(pick.list.pos);
		if (!success) return false;
		pick.sort.root[i].weight = v;
	}

	// using 50-move-counter to determine whether the root position is won, drawn or lost

	auto wdl{ dtz > 0 ? ( dtz + pos.half_count <= 100 ?  2 :  1)
		   : (dtz < 0 ? (-dtz + pos.half_count <= 100 ? -2 : -1) : 0) };

	// determining the score to report

	tb_score = wdl_to_score[wdl + 2];
	if (!uci::syzygy.rule50)
	{
		if (tb_score > SCORE_DRAW) tb_score =  SCORE_TBMATE;
		if (tb_score < SCORE_DRAW) tb_score = -SCORE_TBMATE;
	}

	// the position is winning (or drawn-by-50-move-rule)

	if (dtz > 0)
	{
		int64 best{ 0xffff };
		for (int i{}; i < pick.list.moves; ++i)
		{
			auto v{ pick.sort.root[i].weight };
			if (v > 0 && v < best)
				best = v;
		}
		auto max{ best };

		// if the current phase has not seen repetitions, then trying all moves
		// that stay safely within the 50-move budget, if there are any

		if (!pos.repetition(repetition_hash, uci::move_offset) && best + pos.half_count <= 99)
			max = 99 - pos.half_count;

		for (int i{}; i < pick.list.moves; ++i)
		{
			auto v{ pick.sort.root[i].weight };
			if (v > 0 && v <= max)
				pick.sort.root[i].weight = SCORE_TB - v;
		}
	}

	// the position is loosing

	else if (dtz < 0)
	{
		int64 best{};
		for (int i{}; i < pick.list.moves; ++i)
		{
			auto v{ pick.sort.root[i].weight };
			assert(v < 0);
			if (v < best)
				best = v;
		}

		if (-best * 2 + pos.half_count >= 100)
		{
			// trying to approach a 50-move rule draw

			for (int i{}; i < pick.list.moves; ++i)
			{
				if (pick.sort.root[i].weight == best)
					pick.sort.root[i].weight = SCORE_TB - best;
			}
		}
		else
		{
			for (int i{}; i < pick.list.moves; ++i)
				pick.sort.root[i].weight = -SCORE_TBMATE - pick.sort.root[i].weight;
			return true;
		}
	}

	// the position is a draw and has to be preserved

	else
	{
		for (int i{}; i < pick.list.moves; ++i)
		{
			if (pick.sort.root[i].weight == 0)
				pick.sort.root[i].weight = SCORE_TB;
		}
	}

	// sorting the moves according to their dtz-value

	pick.sort.statical_tb();
	return true;
}

bool syzygy::probe_wdl_root(board &pos, rootpick &pick, int &tb_score)
{
	// using the win-draw-loss-tables to highlight moves that preserve the win or draw
	// this is a fall-back for the case that some or all dtz-tables are missing
	// a return value of false indicates that not all probes were successful and that no moves were marked

	int success{};
	int wdl{ probe_wdl(pos, success) };
	if (!success)
		return false;
	int best{ -2 };

	// determining the score to report

	tb_score = wdl_to_score[wdl + 2];
	if (!uci::syzygy.rule50)
	{
		if (tb_score > SCORE_DRAW) tb_score =  SCORE_TBMATE;
		if (tb_score < SCORE_DRAW) tb_score = -SCORE_TBMATE;
	}

	// probing each move

	for (int i{}; i < pick.list.moves; ++i)
	{
		pos.new_move(pick.sort.root[i].move);
		auto v{ -probe_wdl(pos, success) };
		pos.revert(pick.list.pos);
		if (!success)
			return false;

		pick.sort.root[i].weight = v;
		if (v > best)
			best = v;
	}

	for (int i{}; i < pick.list.moves; ++i)
	{
		if (pick.sort.root[i].weight == best)
			pick.sort.root[i].weight = SCORE_TB;
	}

	// sorting the moves according to their wdl-value

	pick.sort.statical_tb();
	return true;
}
