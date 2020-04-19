/*
  Monolith 2
  Copyright (C) 2017-2020 Jonas Mayr
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


/*
  probing Syzygy endgame tablebases
  all credits go to Ronald de Man for creating the tablebases and providing the probing code:
  https://github.com/syzygy1/tb
  https://github.com/syzygy1/Cfish
  the probing code has been modified to conform with the engine
  DTM tablebases have not been officially released yet, their probing code is therefore not complete
  32-bit is only supported for up to 5-piece tables
*/

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
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

#include "uci.h"
#include "misc.h"
#include "zobrist.h"
#include "movegen.h"
#include "bit.h"
#include "syzygy.h"

// defining globally accessible variables

int syzygy::pc_max{};
int syzygy::tb_cnt{};

// defining limits

namespace lim
{
	constexpr int pc{ lim::syzygy_pieces };
	constexpr int hash_bits{ pc <= 6 ?  11 :  12 };
	constexpr int pc_tb    { pc <= 6 ? 254 : 650 };
	constexpr int pn_tb    { pc <= 6 ? 256 : 861 };
}

// defining types

struct pairs_data
{
	int8* idx_table;
	uint16* size_table;
	uint8* data;
	uint16* offset;
	std::vector<uint8> sym_length;
	uint8* sym_pattern;
	uint8 block_size;
	uint8 idx_bits;
	uint8 min_length;
	uint8 const_value[2];
	std::vector<uint64> base;
};

struct enc_info
{
	pairs_data precomp;
	std::size_t factor[lim::pc];
	uint8 pieces[lim::pc];
	uint8 norm[lim::pc];
};

struct base_entry
{
	key64 key;
	uint8* data[3];
	mem_map mapping[3];
	std::atomic<bool> ready[3];
	uint8 num;
	bool symmetric, has_pawns, has_dtm, has_dtz;
	union { bool kk_enc; uint8 pawns[2]; };
	bool dtm_loss_only;

	base_entry() : ready{ false, false, false } {}
	base_entry(const base_entry&) {}
};

struct piece_entry
{
	base_entry be;
	enc_info ei[2 + 2 + 1];
	uint16* dtm_map;
	uint16  dtm_map_idx[2][2];
	void* dtz_map;
	uint16 dtz_map_idx[4];
	uint8  dtz_flags;
};

struct pawn_entry
{
	base_entry be;
	enc_info ei[4 * 2 + 6 * 2 + 4];
	uint16* dtm_map;
	uint16 dtm_map_idx[6][2][2];
	void* dtz_map;
	uint16 dtz_map_idx[4][4];
	uint8  dtz_flags[4];
	bool dtm_switched;
};

struct hash_entry
{
	key64 key;
	base_entry* ptr;
};

struct count_info
{
	int pc, pn;
	int wdl, dtm, dtz;
};

enum table   : int { wdl, dtm, dtz };
enum encryption : int { enc_pc, enc_fl, enc_rk };

// defining various variables

namespace
{
	// various variables to be initialized before probing

	bool init{};
	count_info cnt{};

	std::string path_string{};
	std::vector<std::string> paths{};

	std::vector<piece_entry> pc_entry{};
	std::vector< pawn_entry> pn_entry{};
	std::array<hash_entry, 1 << lim::hash_bits> tb_hash{};

	// making sure SMP works

	std::mutex mutex{};

	// defining constants & index tables

	const std::string suffix[]{ ".rtbw", ".rtbm", ".rtbz" };
	constexpr key32 magic[]{ 0x5d23e871, 0x88ac504b, 0xa50c66d7 };

	constexpr uint8 pa_flags[]{ 8, 0, 0, 0, 4 };
	constexpr int fl_to_fl[]{ 0, 1, 2, 3, 3, 2, 1, 0 };

	constexpr int wdl_to_score[]{ tb_loss, blessed_loss, draw, cursed_win, tb_win };
	constexpr int wdl_to_map[]{  1,    3, 0,   2, 0 };
	constexpr int wdl_to_dtz[]{ -1, -101, 0, 101, 1 };
}

// encryption arrays

namespace enc
{
	constexpr int diag[]
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

	constexpr int8 off_diag[]
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

	constexpr uint8 flip_diag[]
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

	constexpr int triangle[]
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

	constexpr int lower[]
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

	constexpr uint8 flap[][64]
	{
		{
			 0,  0,  0,  0,  0,  0,  0,  0,
			 0,  6, 12, 18, 18, 12,  6,  0,
			 1,  7, 13, 19, 19, 13,  7,  1,
			 2,  8, 14, 20, 20, 14,  8,  2,
			 3,  9, 15, 21, 21, 15,  9,  3,
			 4, 10, 16, 22, 22, 16, 10,  4,
			 5, 11, 17, 23, 23, 17, 11,  5,
			 0,  0,  0,  0,  0,  0,  0,  0
		},
		{
			 0,  0,  0,  0,  0,  0,  0,  0,
			 0,  1,  2,  3,  3,  2,  1,  0,
			 4,  5,  6,  7,  7,  6,  5,  4,
			 8,  9, 10, 11, 11, 10,  9,  8,
			12, 13, 14, 15, 15, 14, 13, 12,
			16, 17, 18, 19, 19, 18, 17, 16,
			20, 21, 22, 23, 23, 22, 21, 20,
			 0,  0,  0,  0,  0,  0,  0,  0
		}
	};

	constexpr uint8 pawn_twist[][64]
	{
		{
			 0,  0,  0,  0,  0,  0,  0,  0,
			47, 35, 23, 11, 10, 22, 34, 46,
			45, 33, 21,  9,  8, 20, 32, 44,
			43, 31, 19,  7,  6, 18, 30, 42,
			41, 29, 17,  5,  4, 16, 28, 40,
			39, 27, 15,  3,  2, 14, 26, 38,
			37, 25, 13,  1,  0, 12, 24, 36,
			 0,  0,  0,  0,  0,  0,  0,  0
		},
		{
			 0,  0,  0,  0,  0,  0,  0,  0,
			47, 45, 43, 41, 40, 42, 44, 46,
			39, 37, 35, 33, 32, 34, 36, 38,
			31, 29, 27, 25, 24, 26, 28, 30,
			23, 21, 19, 17, 16, 18, 20, 22,
			15, 13, 11,  9,  8, 10, 12, 14,
			 7,  5,  3,  1,  0,  2,  4,  6,
			 0,  0,  0,  0,  0,  0,  0,  0
		}
	};

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

	std::size_t binomial[7][64]{};
	std::size_t pawn_idx[2][6][24]{};
	std::size_t pawnfactor_fl[6][4]{};
	std::size_t pawnfactor_rk[6][6]{};

	void init_indices()
	{
		// initializing indices
		// binomial[k][n] = bin(n, k)

		for (int i{}; i < 7; ++i)
			for (int j{}; j < 64; ++j)
			{
				std::size_t f{ 1 }, l{ 1 };
				for (int k{}; k < i; ++k)
				{
					f *= (j - k);
					l *= (k + 1);
				}
				enc::binomial[i][j] = f / l;
			}

		for (int i{}; i < 6; ++i)
		{
			std::size_t s{};
			for (int j{}; j < 24; ++j)
			{
				enc::pawn_idx[0][i][j] = s;
				s += enc::binomial[i][enc::pawn_twist[0][(1 + (j % 6)) * 8 + (j / 6)]];
				if ((j + 1) % 6 == 0)
				{
					enc::pawnfactor_fl[i][j / 6] = s;
					s = 0;
				}
			}
			s = 0;
			for (int j{}; j < 24; ++j)
			{
				enc::pawn_idx[1][i][j] = s;
				s += enc::binomial[i][enc::pawn_twist[1][(1 + (j / 4)) * 8 + (j % 4)]];
				if ((j + 1) % 4 == 0)
				{
					enc::pawnfactor_rk[i][j / 4] = s;
					s = 0;
				}
			}
		}
	}
}

// tablebase core part

namespace tbcore
{
	FD open_tb(std::string name)
	{
		// opening tablebase

		for (auto& p : paths)
		{
			std::string fullpath{ p + "\\" + name };
			FD fd(memory::open_tb(fullpath));
			if (fd != fd_error) return fd;
		}
		return fd_error;
	}

	bool test_tb(std::string name)
	{
		// testing tablebase

		FD fd{ open_tb(name) };
		if (fd != fd_error)
		{
			auto size{ memory::size_tb(fd) };
			memory::close_tb(fd);
			if ((size & 63) != 16)
			{
				std::cout << "info string warning: incomplete tablebase file: " << name << std::endl;
				fd = fd_error;
			}
		}
		return fd != fd_error;
	}

	void* map_tb(std::string filename, mem_map& map)
	{
		// mapping the table into virtual memory for fast access

		FD fd{ open_tb(filename) };
		if (fd == fd_error)
			return nullptr;

		void* data{ memory::map(fd, map) };
		if (!data)
		{
			std::cout << "info string warning: mapping into memory failed: " << filename << std::endl;
			return nullptr;
		}

		memory::close_tb(fd);
		return data;
	}

	void add_to_hash(base_entry* ptr, key64 key)
	{
		// adding a tb-entry to the hash

		auto idx{ std::size_t(key >> (64 - lim::hash_bits)) };
		while (tb_hash[idx].ptr)
			idx = (idx + 1) & ((1 << lim::hash_bits) - 1);

		tb_hash[idx] = { key, ptr };
	}

	void init(std::vector<int> pieces)
	{
		// initializing the tablebase
		// starting with creating the piece-acronym of the position

		std::string acronym{};
		for (auto p : pieces)
			acronym += "PNBRQK"[p];
		acronym.insert(acronym.find('K', 1), "v");

		if (!test_tb(acronym + suffix[wdl]))
			return;

		// creating material keys

		piece_list pc_list{};
		color cl{ white };
		for (char& ch : acronym)
		{
			if (ch == 'v') { cl = black; continue; }
			for (piece pc : { pawn, knight, bishop, rook, queen, king })
				if ("PNBRQK"[pc] == ch)
					pc_list[cl][pc] += 1;
		}

		key64 key1{ zobrist::mat_key<piece_list>(pc_list, false) };
		key64 key2{ zobrist::mat_key<piece_list>(pc_list,  true) };

		// creating a new tablebase-entry & hashing it

		bool has_pawns{ pc_list[white][pawn] || pc_list[black][pawn] };
		base_entry* be{ has_pawns ? &pn_entry[cnt.pn++].be : &pc_entry[cnt.pc++].be };

		be->has_pawns = has_pawns;
		be->key = key1;
		be->symmetric = (key1 == key2);
		be->num = 0;
		for (auto& cl : pc_list)
			for (auto& pc : cl)
				be->num += pc;
		for (auto& ready : be->ready)
			ready = false;

		cnt.wdl += 1;
		cnt.dtm += be->has_dtm = test_tb(acronym + suffix[dtm]);
		cnt.dtz += be->has_dtz = test_tb(acronym + suffix[dtz]);
		syzygy::pc_max = std::max(syzygy::pc_max, int(be->num));

		if (be->has_pawns)
		{
			be->pawns[white] = pc_list[white][pawn];
			be->pawns[black] = pc_list[black][pawn];

			if (pc_list[black][pawn] && (!pc_list[white][pawn] || pc_list[black][pawn] < pc_list[white][pawn]))
				std::swap(be->pawns[white], be->pawns[black]);
		}
		else
		{
			int pc_cnt{};
			for (auto& cl : pc_list)
				for (auto& pc : cl)
					if (pc == 1) pc_cnt += 1;
			be->kk_enc = (pc_cnt == 2);
		}
		add_to_hash(be, key1);
		if (key2 != key1)
			add_to_hash(be, key2);
	}

	int cnt_tables(base_entry* be, table type)
	{
		return be->has_pawns ? type == dtm ? 6 : 4 : 1;
	}

	enc_info* first_ei(base_entry* be, table type)
	{
		return be->has_pawns
			?  &((pawn_entry*)be)->ei[type == wdl ? 0 : type == dtm ? 8 : 20]
			: &((piece_entry*)be)->ei[type == wdl ? 0 : type == dtm ? 2 :  4];
	}

	void free_tb_entry(base_entry* be)
	{
		for (table type : { wdl, dtm, dtz })
		{
			if (be->ready[type].load(std::memory_order_relaxed))
			{
				memory::unmap(be->data[type], be->mapping[type]);
				be->ready[type].store(false, std::memory_order_relaxed);
			}
		}
	}

	int leading_pawn(int p[], base_entry* entry, encryption enc)
	{
		// determining file of leftmost pawn and sort pawns

		for (int i{ 1 }; i < entry->pawns[0]; ++i)
			if (enc::flap[enc - 1][p[0]] > enc::flap[enc - 1][p[i]])
				std::swap(p[0], p[i]);

		return enc == enc_fl ? fl_to_fl[type::fl_of(square(p[0]))] : type::rk_of(square(p[0] - 8));
	}

	std::size_t encode(int p[], enc_info* ei, base_entry* be, encryption enc)
	{
		int n = be->num;
		std::size_t idx;
		int k;

		if (p[0] & 0x04)
			for (int i = 0; i < n; i++)
				p[i] ^= 0x07;

		if (enc == enc_pc)
		{
			if (p[0] & 0x20)
				for (int i = 0; i < n; i++)
					p[i] ^= 0x38;

			for (int i = 0; i < n; i++)
				if (enc::off_diag[p[i]])
				{
					if (enc::off_diag[p[i]] > 0 && i < (be->kk_enc ? 2 : 3))
						for (int j = 0; j < n; j++)
							p[j] = enc::flip_diag[p[j]];
					break;
				}

			if (be->kk_enc)
			{
				idx = enc::kk_map[enc::triangle[p[0]]][p[1]];
				k = 2;
			}
			else
			{
				int s1 = (p[1] > p[0]);
				int s2 = (p[2] > p[0]) + (p[2] > p[1]);

				if (enc::off_diag[p[0]])
					idx = enc::triangle[p[0]] * 63 * 62 + (p[1] - s1) * 62 + (p[2] - s2);
				else if (enc::off_diag[p[1]])
					idx = 6 * 63 * 62 + enc::diag[p[0]] * 28 * 62 + enc::lower[p[1]] * 62 + p[2] - s2;
				else if (enc::off_diag[p[2]])
					idx = 6 * 63 * 62 + 4 * 28 * 62 + enc::diag[p[0]] * 7 * 28 + (enc::diag[p[1]] - s1) * 28 + enc::lower[p[2]];
				else
					idx = 6 * 63 * 62 + 4 * 28 * 62 + 4 * 7 * 28 + enc::diag[p[0]] * 7 * 6 + (enc::diag[p[1]] - s1) * 6 + (enc::diag[p[2]] - s2);
				k = 3;
			}
			idx *= ei->factor[0];
		}
		else
		{
			for (int i = 1; i < be->pawns[0]; i++)
				for (int j = i + 1; j < be->pawns[0]; j++)
					if (enc::pawn_twist[enc - 1][p[i]] < enc::pawn_twist[enc - 1][p[j]])
						std::swap(p[i], p[j]);

			k = be->pawns[0];
			idx = enc::pawn_idx[enc - 1][k - 1][enc::flap[enc - 1][p[0]]];
			for (int i = 1; i < k; i++)
				idx += enc::binomial[k - i][enc::pawn_twist[enc - 1][p[i]]];
			idx *= ei->factor[0];

			// pawns of other color

			if (be->pawns[1])
			{
				int t = k + be->pawns[1];
				for (int i = k; i < t; i++)
					for (int j = i + 1; j < t; j++)
						if (p[i] > p[j]) std::swap(p[i], p[j]);
				std::size_t s = 0;
				for (int i = k; i < t; i++)
				{
					int sq = p[i];
					int skips = 0;
					for (int j = 0; j < k; j++)
						skips += (sq > p[j]);
					s += enc::binomial[i - k + 1][sq - skips - 8];
				}
				idx += s * ei->factor[k];
				k = t;
			}
		}
		for (; k < n;)
		{
			int t = k + ei->norm[k];
			for (int i = k; i < t; i++)
				for (int j = i + 1; j < t; j++)
					if (p[i] > p[j]) std::swap(p[i], p[j]);
			std::size_t s = 0;

			for (int i = k; i < t; i++)
			{
				int sq = p[i];
				int skips = 0;
				for (int j = 0; j < k; j++)
					skips += (sq > p[j]);
				s += enc::binomial[i - k + 1][sq - skips];
			}
			idx += s * ei->factor[k];
			k = t;
		}

		return idx;
	}

	std::size_t subfactor(std::size_t k, std::size_t n)
	{
		// counting placements of k like pieces on n squares

		std::size_t f{ n }, l{ 1 };
		for (std::size_t i{ 1 }; i < k; ++i)
		{
			f *= n - i;
			l *= i + 1;
		}
		return f / l;
	}

	std::size_t init_enc_info(enc_info* ei, base_entry* be, uint8 tb[], int shift, int t, encryption enc)
	{
		bool morePawns = enc != enc_pc && be->pawns[1] > 0;

		for (int i = 0; i < be->num; i++)
		{
			ei->pieces[i] = (tb[i + 1 + morePawns] >> shift) & 0x0f;
			ei->norm[i] = 0;
		}

		int order = (tb[0] >> shift) & 0x0f;
		int order2 = morePawns ? (tb[1] >> shift) & 0x0f : 0x0f;

		int k = ei->norm[0] = enc != enc_pc ? be->pawns[0] : be->kk_enc ? 2 : 3;

		if (morePawns)
		{
			ei->norm[k] = be->pawns[1];
			k += ei->norm[k];
		}

		for (int i = k; i < be->num; i += ei->norm[i])
			for (int j = i; j < be->num && ei->pieces[j] == ei->pieces[i]; j++)
				ei->norm[i]++;

		int n = 64 - k;
		std::size_t f = 1;

		for (int i = 0; k < be->num || i == order || i == order2; i++)
		{
			if (i == order)
			{
				ei->factor[0] = f;
				f *=  enc == enc_fl ? enc::pawnfactor_fl[ei->norm[0] - 1][t]
					: enc == enc_rk ? enc::pawnfactor_rk[ei->norm[0] - 1][t]
					: be->kk_enc ? 462 : 31332;
			}
			else if (i == order2)
			{
				ei->factor[ei->norm[0]] = f;
				f *= subfactor(ei->norm[ei->norm[0]], 48 - ei->norm[0]);
			}
			else
			{
				ei->factor[k] = f;
				f *= subfactor(ei->norm[k], n);
				n -= ei->norm[k];
				k += ei->norm[k];
			}
		}

		return f;
	}

	void symbol_length(pairs_data& d, int s, std::vector<bool>& tmp)
	{
		// calculating the length of symbols for Huffmann-compression

		[[maybe_unused]] int max_s{ (int)d.sym_length.size() };
		verify(s < max_s);
		uint8* w{ d.sym_pattern + 3 * s };
		int s2{ (w[2] << 4) | (w[1] >> 4) };
		if (s2 == 0x0fff)
			d.sym_length[s] = 0;
		else
		{
			int s1{ ((w[1] & 0xf) << 8) | w[0] };
			if (!tmp[s1]) symbol_length(d, s1, tmp);
			if (!tmp[s2]) symbol_length(d, s2, tmp);
			verify(s1 < max_s);
			verify(s2 < max_s);
			d.sym_length[s] = d.sym_length[s1] + d.sym_length[s2] + 1;
		}
		tmp[s] = 1;
	}

	void setup_pairs(pairs_data& d, uint8** ptr, std::size_t tb_size, std::size_t size[], uint8& flags, table type)
	{
		uint8* data = *ptr;

		flags = data[0];
		if (data[0] & 0x80)
		{
			d.idx_bits = 0;
			d.const_value[0] = type == wdl ? data[1] : 0;
			d.const_value[1] = 0;
			*ptr = data + 2;
			size[0] = size[1] = size[2] = 0;
			return;
		}

		uint8 blockSize = data[1];
		uint8 idxBits = data[2];
		uint32 realNumBlocks = bit::read_le<uint32>(&data[4]);
		uint32 numBlocks = realNumBlocks + data[3];
		int maxLen = data[8];
		int minLen = data[9];
		int h = maxLen - minLen + 1;
		uint32 numSyms = bit::read_le<uint16>(&data[10 + 2 * h]);
		d.block_size = blockSize;
		d.idx_bits = idxBits;
		d.offset = (uint16*)&data[10];

		d.sym_length.resize(numSyms);
		d.sym_pattern = &data[12 + 2 * h];
		d.min_length = minLen;
		*ptr = &data[12 + 2 * h + 3 * numSyms + (numSyms & 1)];

		std::size_t num_indices = (tb_size + (1ULL << idxBits) - 1) >> idxBits;
		size[0] = 6ULL * num_indices;
		size[1] = 2ULL * numBlocks;
		size[2] = (std::size_t)realNumBlocks << blockSize;

		std::vector<bool> tmp(numSyms);
		for (uint32 s{}; s < numSyms; ++s)
			if (!tmp[s])
				symbol_length(d, s, tmp);

		d.base.resize(h);
		d.base[h - 1] = 0;
		for (int i = h - 2; i >= 0; i--)
			d.base[i] = (d.base[i + 1] + bit::read_le<uint16>((uint8*)(d.offset + i))
				- bit::read_le<uint16>((uint8*)(d.offset + i + 1))) / 2;
		for (int i = 0; i < h; i++)
			d.base[i] <<= uint64(64 - (minLen + i));

		d.offset -= d.min_length;

		return;
	}

	bool init_table(base_entry* be, std::string acronym, table type)
	{
		uint8* data = (uint8*)map_tb(acronym + suffix[type], be->mapping[type]);
		if (!data) return false;

		if (bit::read_le<uint32>(data) != magic[type])
		{
			std::cout << "info string warning: corrupted table" << std::endl;
			memory::unmap(data, be->mapping[type]);
			return false;
		}

		be->data[type] = data;

		bool split = type != dtz && (data[4] & 0x01);
		if (type == dtm)
			be->dtm_loss_only = data[4] & 0x04;

		data += 5;

		std::size_t tb_size[6][2];
		int num = cnt_tables(be, type);
		enc_info* ei = first_ei(be, type);
		encryption enc = !be->has_pawns ? enc_pc : type != dtm ? enc_fl : enc_rk;

		for (int t = 0; t < num; t++)
		{
			tb_size[t][0] = init_enc_info(&ei[t], be, data, 0, t, enc);
			if (split)
				tb_size[t][1] = init_enc_info(&ei[num + t], be, data, 4, t, enc);
			data += be->num + 1 + (be->has_pawns && be->pawns[1]);
		}
		data += (uintptr_t)data & 1;

		std::size_t size[6][2][3];
		for (int t = 0; t < num; t++)
		{
			uint8 flags;
			setup_pairs(ei[t].precomp, &data, tb_size[t][0], size[t][0], flags, type);

			if (type == dtz)
			{
				if (!be->has_pawns)
					((piece_entry*)be)->dtz_flags = flags;
				else
					((pawn_entry*)be)->dtz_flags[t] = flags;
			}
			if (split)
				setup_pairs(ei[num + t].precomp, &data, tb_size[t][1], size[t][1], flags, type);
			else if (type != dtz)
				ei[num + t].precomp = pairs_data{};
		}

		if (type == dtm && !be->dtm_loss_only)
		{
			uint16* map = (uint16*)data;
			*(be->has_pawns ? &((pawn_entry*)be)->dtm_map : &((piece_entry*)be)->dtm_map) = map;
			uint16(*mapIdx)[2][2] = be->has_pawns ? &((pawn_entry*)be)->dtm_map_idx[0] : &((piece_entry*)be)->dtm_map_idx;
			for (int t = 0; t < num; t++)
			{
				for (int i = 0; i < 2; i++)
				{
					mapIdx[t][0][i] = uint16((uint16*)data + 1 - map);
					data += 2 + 2 * bit::read_le<uint16>(data);
				}
				if (split)
				{
					for (int i = 0; i < 2; i++) {
						mapIdx[t][1][i] = uint16((uint16*)data + 1 - map);
						data += 2 + 2 * bit::read_le<uint16>(data);
					}
				}
			}
		}

		if (type == dtz)
		{
			void* map = data;
			*(be->has_pawns ? &((pawn_entry*)be)->dtz_map : &((piece_entry*)be)->dtz_map) = map;
			uint16(*mapIdx)[4] = be->has_pawns ? &((pawn_entry*)be)->dtz_map_idx[0] : &((piece_entry*)be)->dtz_map_idx;
			uint8* flags = be->has_pawns ? &((pawn_entry*)be)->dtz_flags[0] : &((piece_entry*)be)->dtz_flags;
			for (int t = 0; t < num; t++)
			{
				if (flags[t] & 2)
				{
					if (!(flags[t] & 16))
						for (int i = 0; i < 4; i++)
						{
							mapIdx[t][i] = uint16(data + 1 - (uint8*)map);
							data += 1 + data[0];
						}
					else
					{
						data += (uintptr_t)data & 0x01;
						for (int i = 0; i < 4; i++)
						{
							mapIdx[t][i] = uint16((uint16*)data + 1 - (uint16*)map);
							data += 2 + 2 * bit::read_le<uint16>(data);
						}
					}
				}
			}
			data += (uintptr_t)data & 0x01;
		}

		for (int t = 0; t < num; t++)
		{
			ei[t].precomp.idx_table = (int8*)data;
			data += size[t][0][0];
			if (split)
			{
				ei[num + t].precomp.idx_table = (int8*)data;
				data += size[t][1][0];
			}
		}
		for (int t = 0; t < num; t++)
		{
			ei[t].precomp.size_table = (uint16*)data;
			data += size[t][0][1];
			if (split)
			{
				ei[num + t].precomp.size_table = (uint16*)data;
				data += size[t][1][1];
			}
		}
		for (int t = 0; t < num; t++)
		{
			data = (uint8*)(((uintptr_t)data + 0x3f) & ~0x3f);
			ei[t].precomp.data = data;
			data += size[t][0][2];
			if (split)
			{
				data = (uint8*)(((uintptr_t)data + 0x3f) & ~0x3f);
				ei[num + t].precomp.data = data;
				data += size[t][1][2];
			}
		}

		if (type == dtm && be->has_pawns)
			((pawn_entry*)be)->dtm_switched = zobrist::mat_key(ei[0].pieces, be->num) != be->key;

		return true;
	}

	uint8* decompress_pairs(pairs_data& d, std::size_t idx)
	{
		if (!d.idx_bits)
			return d.const_value;

		uint32 mainIdx = uint32(idx >> d.idx_bits);
		int litIdx = (idx & (((std::size_t)1 << d.idx_bits) - 1)) - ((std::size_t)1 << (d.idx_bits - 1));

		uint32 block{ *((uint32*)(d.idx_table + 6 * mainIdx)) };
		block = bit::le<uint32>(block);

		uint16 idxOffset = *(uint16*)(d.idx_table + 6 * mainIdx + 4);
		litIdx += bit::le<uint16>(idxOffset);

		if (litIdx < 0)
			while (litIdx < 0)
				litIdx += d.size_table[--block] + 1;
		else
			while (litIdx > d.size_table[block])
				litIdx -= d.size_table[block++] + 1;

		uint32* ptr = (uint32*)(d.data + ((std::size_t)block << d.block_size));

		int m = d.min_length;
		uint16* offset = d.offset;
		uint32 sym, bitCnt;
		uint64 code = bit::be<uint64>(*(uint64*)ptr);

		// number of "empty bits" in code
		bitCnt = 0;

		ptr += 2;
		for (;;) {
			int l = m;
			while (code < d.base[l - m]) l++;
			sym = bit::le<uint16>(offset[l]);
			sym += uint32((code - d.base[l - m]) >> (64 - l));
			if (litIdx < (int)d.sym_length[sym] + 1) break;
			litIdx -= (int)d.sym_length[sym] + 1;
			code <<= l;
			bitCnt += l;
			if (bitCnt >= 32) {
				bitCnt -= 32;
				uint32 tmp = bit::be<uint32>(*ptr++);
				code |= (uint64)tmp << bitCnt;
			}
		}

		uint8* symPat = d.sym_pattern;
		while (d.sym_length[sym] != 0) {
			uint8* w = symPat + (3 * sym);
			int s1 = ((w[1] & 0xf) << 8) | w[0];
			if (litIdx < (int)d.sym_length[s1] + 1)
				sym = s1;
			else {
				litIdx -= (int)d.sym_length[s1] + 1;
				sym = (w[2] << 4) | (w[1] >> 4);
			}
		}

		return &symPat[3 * sym];
	}

	int fill_squares(board& pos, uint8 pc[], int flip, int mirror, int p[], int i)
	{
		// p[i] is to contain the square for a piece of type pc[i] ^ flip,
		// where pieces are encoded as 1-6 for white pawn-king and 9-14 for black pawn-king
		// pc ^ flip flips between white and black if flip == true
		// pieces of the same type are guaranteed to be consecutive.

		uint64 bb{ pos.side[(pc[i] >> 3) ^ flip] & pos.pieces[(pc[i] & 7) - 1] };
		do {
			p[i++] = bit::scan(bb) ^ mirror;
			bb &= bb - 1;
		} while (bb);
		return i;
	}
}

// tablebase probing part

namespace syzygy
{
	std::string create_acronym(board& pos, bool mirror)
	{
		// producing an acronym text string of the position, e.g. 'KQPvKRP'

		verify(mirror == white || mirror == black);

		std::string acronym{};
		color cl{ mirror ? black : white };
		while (true)
		{
			for (piece pc : { king, queen, rook, bishop, knight, pawn})
				acronym += std::string(bit::popcnt(pos.pieces[pc] & pos.side[cl]), "PNBRQK"[pc]);
			if (mirror != bool(cl)) break;
			acronym += "v";
			cl ^= 1;
		}
		return acronym;
	}

	int probe_table(board& pos, int s, int& success, table type)
	{
		// probing a table
		// tables don't contain information for positions with active castling rights

		for (color cl : { white, black })
			for (flag fl : { castle_east, castle_west })
				if (pos.castle_right[cl][fl] != prohibited)
				{
					success = 0;
					return 0;
				}

		// aborting also if KvK

		if (type == wdl && pos.pieces[king] == pos.side[both])
			return 0;

		// obtaining the position's material-signature key

		key64 key{ zobrist::mat_key<board>(pos, false) };

		int hashIdx = key >> (64 - lim::hash_bits);
		while (tb_hash[hashIdx].key && tb_hash[hashIdx].key != key)
			hashIdx = (hashIdx + 1) & ((1 << lim::hash_bits) - 1);
		if (!tb_hash[hashIdx].ptr)
		{
			success = 0;
			return 0;
		}

		base_entry* be = tb_hash[hashIdx].ptr;
		if ((type == dtm && !be->has_dtm) || (type == dtz && !be->has_dtz)) {
			success = 0;
			return 0;
		}

		// Use double-checked locking to reduce locking overhead

		if (!be->ready[type].load(std::memory_order_acquire))
		{
			std::unique_lock<std::mutex> lock(mutex);
			if (!be->ready[type].load(std::memory_order_relaxed))
			{
				std::string acronym{ create_acronym(pos, be->key != key) };
				if (!tbcore::init_table(be, acronym, type))
				{
					// marking table as deleted

					tb_hash[hashIdx].ptr = nullptr;
					success = 0;
					return 0;
				}
				be->ready[type].store(true, std::memory_order_release);
			}
		}

		bool bside, flip;
		if (!be->symmetric) {
			flip = key != be->key;
			bside = (pos.cl == white) == flip;
			if (type == dtm && be->has_pawns && ((pawn_entry*)be)->dtm_switched) {
				flip = !flip;
				bside = !bside;
			}
		}
		else {
			flip = pos.cl != white;
			bside = false;
		}

		enc_info* ei = tbcore::first_ei(be, type);

		int p[lim::pc];
		std::size_t idx;
		int t = 0;
		uint8 flags{};

		if (!be->has_pawns)
		{
			if (type == dtz) {
				flags = ((piece_entry*)be)->dtz_flags;
				if ((flags & 1) != bside && !be->symmetric) {
					success = -1;
					return 0;
				}
			}
			ei = type != dtz ? &ei[bside] : ei;
			for (int i = 0; i < be->num;)
				i = tbcore::fill_squares(pos, ei->pieces, flip, 0, p, i);
			idx = tbcore::encode(p, ei, be, enc_pc);
		}
		else
		{
			int i = tbcore::fill_squares(pos, ei->pieces, flip, flip ? 0x38 : 0, p, 0);
			t = tbcore::leading_pawn(p, be, type != dtm ? enc_fl : enc_rk);

			if (type == dtz) {
				flags = ((pawn_entry*)be)->dtz_flags[t];
				if ((flags & 1) != bside && !be->symmetric) {
					success = -1;
					return 0;
				}
			}
			ei = type == wdl ? &ei[t + 4 * bside]
				: type == dtm ? &ei[t + 6 * bside] : &ei[t];

			while (i < be->num)
				i = tbcore::fill_squares(pos, ei->pieces, flip, flip ? 0x38 : 0, p, i);
			idx = type != dtm ? tbcore::encode(p, ei, be, enc_fl) : tbcore::encode(p, ei, be, enc_rk);
		}

		uint8* w = tbcore::decompress_pairs(ei->precomp, idx);

		if (type == wdl)
			return (int)w[0] - 2;

		int v = w[0] + ((w[1] & 0x0f) << 8);

		if (type == dtm) {
			if (!be->dtm_loss_only)
				v = bit::le<uint16>(be->has_pawns
					? ((pawn_entry*)be)->dtm_map[((pawn_entry*)be)->dtm_map_idx[t][bside][s] + v]
					: ((piece_entry*)be)->dtm_map[((piece_entry*)be)->dtm_map_idx[bside][s] + v]);
		}
		else {
			if (flags & 2) {
				int m = wdl_to_map[s + 2];
				if (!(flags & 16))
					v = be->has_pawns
					? ((uint8*)((pawn_entry*)be)->dtz_map)[((pawn_entry*)be)->dtz_map_idx[t][m] + v]
					: ((uint8*)((piece_entry*)be)->dtz_map)[((piece_entry*)be)->dtz_map_idx[m] + v];
				else
					v = bit::le<uint16>(be->has_pawns
						? ((uint16*)((pawn_entry*)be)->dtz_map)[((pawn_entry*)be)->dtz_map_idx[t][m] + v]
						: ((uint16*)((piece_entry*)be)->dtz_map)[((piece_entry*)be)->dtz_map_idx[m] + v]);
			}
			if (!(flags & pa_flags[s + 2]) || (s & 1))
				v *= 2;
		}
		return v;
	}

	int probe_wdl_table(board& pos, int& success)
	{
		return probe_table(pos, 0, success, wdl);
	}

	static int probe_dtz_table(board& pos, int wdl, int& success)
	{
		return probe_table(pos, wdl, success, dtz);
	}

	int alphabeta(board& pos, int alpha, int beta, int& success)
	{
		// doing a small alpha-beta search for positions with en-passant captures
		// generating all legal captures including all promotions

		int sc{};
		gen<mode::legal> list(pos);
		list.gen_capture();
		list.gen_promo(stage::promo_all);

		board prev_pos(pos);
		for (int i{}; i < list.cnt.mv; ++i)
		{
			verify_deep(pos.pseudolegal(list.mv[i]));
			if (!list.mv[i].capture())
				continue;

			pos.new_move(list.mv[i]);
			verify_deep(pos.legal());

			sc = -alphabeta(pos, -beta, -alpha, success);
			pos = prev_pos;
			if (success == 0) return 0;
			if (sc > alpha)
			{
				if (sc >= beta)
					return sc;
				alpha = sc;
			}
		}

		sc = probe_wdl_table(pos, success);
		return alpha >= sc ? alpha : sc;
	}
}

void syzygy::init_tb(const std::string& path)
{
	// initializing all the tablebases that can be found in <path>

	if (!::init)
	{
		enc::init_indices();
		::init = true;
	}

	// cleaning up the path string

	if (!path_string.empty())
	{
		path_string.clear();
		paths.clear();

		for (auto& e : pc_entry)
			tbcore::free_tb_entry((base_entry*)&e);
		for (auto& e : pn_entry)
			tbcore::free_tb_entry((base_entry*)&e);
		pc_entry.clear();
		pn_entry.clear();
	}

	// returning immediately if path is an empty string or equals "<empty>"

	path_string = path;
	if (path_string.empty() || path_string == "<empty>") return;

#if !defined(_WIN32)
	char sep{ ':' };
#else
	char sep{ ';' };
#endif

	for (uint32 i{ 1 }, j{}; i < path_string.size(); ++i)
	{
		if (i + 1 == path_string.size() || path_string[i + 1] == sep)
		{
			paths.push_back(path_string.substr(j, i + 1 - j));
			j = i + 2;
		}
	}

	pc_max = tb_cnt = 0;
	cnt = {};
	for (hash_entry& hash : tb_hash)
		hash = hash_entry{ 0ULL, nullptr };

	// allocating heap memory for the tablebase entries
	// ~1 MB for all tables up to 6 pieces, ~4 MB including 7 piece tables

	if (pc_entry.empty())
	{
		verify(pn_entry.empty());
		try
		{
			pc_entry.resize(lim::pc_tb);
			pn_entry.resize(lim::pn_tb);
		}
		catch (std::bad_alloc&)
		{
			std::cout << "info string warning: memory allocation for tablebases failed" << std::endl;
			pc_entry.clear();
			pn_entry.clear();
			return;
		}
	}

	// initializing all possible tables

	for (int p1{ pawn }; p1 <= queen; ++p1)
	{
		tbcore::init({ king, p1, king });
		for (int p2{ pawn }; p2 <= p1; ++p2)
		{
			tbcore::init({ king, p1, king, p2 });
			tbcore::init({ king, p1, p2, king });
			for (int p3{ pawn }; p3 <= queen; ++p3)
				tbcore::init({ king, p1, p2, king, p3 });

			for (int p3{ pawn }; p3 <= p2; ++p3)
			{
				tbcore::init({ king, p1, p2, p3, king });
				for (int p4{ pawn }; p4 <= queen; ++p4)
				{
					tbcore::init({ king, p1, p2, p3, king, p4 });
					if constexpr (lim::pc > 6)
						for (int p5{ pawn }; p5 <= p4; ++p5)
							tbcore::init({ king, p1, p2, p3, king, p4, p5 });
				}
				for (int p4{ pawn }; p4 <= p3; ++p4)
				{
					tbcore::init({ king, p1, p2, p3, p4, king });
					if constexpr (lim::pc > 6)
					{
						for (int p5{ pawn }; p5 <= queen; ++p5)
							tbcore::init({ king, p1, p2, p3, p4, king, p5 });
						for (int p5{ pawn }; p5 <= p4; ++p5)
							tbcore::init({ king, p1, p2, p3, p4, p5, king });
					}
				}
			}
			for (int p3{ pawn }; p3 <= p1; ++p3)
				for (int p4{ pawn }; p4 <= (p1 == p3 ? p2 : p3); ++p4)
					tbcore::init({ king, p1, p2, king, p3, p4 });
		}
	}
	tb_cnt = cnt.pc + cnt.pn;
	std::cout << "info string tablebases found: " << cnt.dtz << " DTZ, " << cnt.wdl << " WDL" << std::endl;
}

int syzygy::probe_wdl(board& pos, int& success)
{
	// probing the Win-Draw-Loss table
	// if success != 0, the probe was successful
	// if success == 2, the position has a winning capture, or the position is a cursed win and has a cursed winning capture,
	// or the position has an en-passant capture as only best move

	success = 1;

	// generating all subsequent legal captures including promotions

	gen<mode::legal> list(pos);
	list.gen_capture();
	list.gen_promo(stage::promo_all);
	int best_capture{ -3 };
	int best_ep{ -3 };

	// keeping track of the best capture & still better en-passant capture of the subsequent moves

	for (int i{}; i < list.cnt.mv; ++i)
	{
		verify_deep(pos.pseudolegal(list.mv[i]));
		if (!list.mv[i].capture())
			continue;

		pos.new_move(list.mv[i]);
		verify_deep(pos.legal());

		int sc{ -alphabeta(pos, -2, -best_capture, success) };
		pos = list.pos;
		if (!success)
			return 0;
		if (sc > best_capture)
		{
			if (sc == 2)
			{
				success = 2;
				return 2;
			}
			if (list.mv[i].fl() != enpassant)
				best_capture = sc;
			else if (sc > best_ep)
				best_ep = sc;
		}
	}

	int wdl{ probe_wdl_table(pos, success) };
	if (!success)
		return 0;

	// now max(wdl, best_capture) is the WDL value of the position without en-passant rights
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

		int mv_cnt{}, ep_cnt{}, i{};
		for (; i < list.cnt.mv; ++i)
		{
			if (list.mv[i].fl() == enpassant)
			{
				ep_cnt += 1;
				continue;
			}
			mv_cnt += 1;
			break;
		}
		if (mv_cnt == 0 && !pos.check())
		{
			verify(list.cnt.mv == ep_cnt);
			verify(list.cnt.mv == i);
			list.gen_quiet();
			verify(list.cnt.promo == 0);
			verify(list.cnt.capture == i);
			mv_cnt += i < list.cnt.mv;
		}
		if (mv_cnt == 0)
		{
			// stalemate

			success = 2;
			return best_ep;
		}
	}

	// stalemate & en-passant are not an issue, so WDL is correct

	return wdl;
}

int syzygy::probe_dtz(board& pos, int& success)
{
	// probing the Distance-To-Zero table for a particular position
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
	// this means that if DTZ > 0 is returned, the position is certainly a win if DTZ + 50-move-counter <= 99
	// care must be taken that the engine picks moves that preserve DTZ + 50-move-counter <= 99
	// if n = 100 immediately after a capture or pawn move, then the position is also certainly a win,
	// and during the whole phase until the next capture or pawn move, the inequality to be preserved is
	// DTZ + 50-move-counter <= 100

	int wdl{ probe_wdl(pos, success) };
	if (!success)
		return 0;

	// draw, DTZ = 0

	if (wdl == 0)
		return 0;

	// checking for winning (cursed) capture or en-passant capture as only best move

	if (success == 2)
		return wdl_to_dtz[wdl + 2];

	// checking for a winning pawn move if the position is winning

	gen<mode::legal> list(pos);
	if (wdl > 0)
	{
		// generating all legal quiet pawn moves including non-capturing promotions

		list.gen_all();
		for (int i{}; i < list.cnt.mv; ++i)
		{
			if (list.mv[i].pc() != pawn || list.mv[i].capture())
				continue;

			pos.new_move(list.mv[i]);
			auto v{ -probe_wdl(pos, success) };
			pos = list.pos;
			if (success == 0)
				return 0;
			if (v == wdl)
				return wdl_to_dtz[v + 2];
		}
	}

	// the best move can not be an en-passant capture now
	// in other words, the value of WDL corresponds to the WDL value of the position without en-passant rights
	// it is therefore safe to probe the DTZ table with the current value of WDL

	int dtz{ probe_dtz_table(pos, wdl, success) };
	if (success >= 0)
		return wdl_to_dtz[wdl + 2] + ((wdl > 0) ? dtz : -dtz);

	// success < 0 means that the DTZ-table needs to be probed by the other side to move

	int best{};
	if (wdl > 0)
		best = std::numeric_limits<int32>::max();
	else
	{
		// if (cursed) loss, the worst case is a losing capture or pawn move as the "best" move,
		// leading to DTZ of -1 or -101. In case of mate, this will cause -1 to be returned

		best = wdl_to_dtz[wdl + 2];
		verify(list.empty());
		list.gen_all();
	}

	for (int i{}; i < list.cnt.mv; ++i)
	{
		// pawn moves and captures can be skipped, because if WDL > 0 they were already caught,
		// and if WDL < 0 the initial value of best already takes account of them

		if (list.mv[i].capture() || list.mv[i].pc() == pawn)
			continue;

		pos.new_move(list.mv[i]);
		int v{ -probe_dtz(pos, success) };

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
		if (v == 1 && pos.check())
		{
			// mate

			gen<mode::legal> list2(pos);
			list2.gen_all();
			if (list2.empty())
				best = 1;
		}
		pos = list.pos;
		if (success == 0)
			return 0;
	}
	return best;
}

bool syzygy::probe_dtz_root(board& pos, rootpick& pick, const std::array<key64, 256>& rep_hash)
{
	// using the Distance-To-Zero tables to weight all root moves

	int success{};
	(void)probe_dtz(pos, success);
	if (!success)
		return false;

	// probing each move

	bool repetition{ pos.repetition(rep_hash, uci::mv_offset) };
	for (auto node{ pick.first() }; node; node = pick.next())
	{
		pos.new_move(node->mv);
		int sc{};

		if (pos.half_cnt == 0)
		{
			// if the move resets the 50-move counter, WDL tables are probed and the WDL score converted to DTZ scores

			int wdl{ -probe_wdl(pos, success) };
			sc = wdl_to_dtz[wdl + 2];
		}
		else
		{
			// otherwise, probing DTZ for the new position and correcting by 1 ply

			sc = -probe_dtz(pos, success);
			if      (sc > 0) sc += 1;
			else if (sc < 0) sc -= 1;
		}

		if (pos.check() && sc == 2)
		{
			// making sure that a mating move gets a score of 1

			gen<mode::legal> list(pos);
			list.gen_all();
			if (list.empty())
				sc = 1;
		}

		pick.revert(pos);
		if (!success)
			return false;

		// calculating the final score of the move

		int s{ sc > 0 ? (pos.half_cnt + sc <= 100 && !repetition ? tb_win  - sc : draw + sc / 10)
		     : sc < 0 ? (pos.half_cnt - sc <= 100                ? tb_loss - sc : draw + sc / 10)
			 : draw };
		node->weight = (int64)s;
	}

	pick.sort_tb();
	return true;
}

bool syzygy::probe_wdl_root(board& pos, rootpick& pick)
{
	// using the Win-Draw-Loss tables to weight all root moves
	// this is a fall-back for the case that some or all DTZ tables are missing

	int success{};
	(void)probe_wdl(pos, success);
	if (!success)
		return false;

	// probing each move and assigning a score

	for (auto node{ pick.first() }; node; node = pick.next())
	{
		pos.new_move(node->mv);
		int sc{ -probe_wdl(pos, success) };
		pick.revert(pos);
		if (!success)
			return false;
		node->weight = (int64)wdl_to_score[sc + 2];
	}

	pick.sort_tb();
	return true;
}
