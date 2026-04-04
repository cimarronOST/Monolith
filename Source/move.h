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

#include <array>
#include <string>

#include "types.h"

using castle_sq = std::array<std::array<square, 2>, 2>;

// arranging the internal move encoding

class move
{
private:
	// all move details are compressed into 22 bits which are stored in 4 bytes
	// the remaining 10 bits are empty

	uint32 mv{};
	static bool verify_move(square sq1, square sq2, piece pc, piece vc, color cl, flag fl);

public:
	move() {}
	move(uint32 raw_mv) : mv{ raw_mv } {}
	move(square sq1, square sq2, piece pc, piece vc, color cl, flag fl);

	uint32 raw() const;
	void set_sq2(square sq);

	operator bool() const;
	bool operator==(const move& m) const;
	bool operator!=(const move& m) const;

	// itemizing the move

	square sq1() const;
	square sq2() const;
	piece  pc()  const;
	piece  vc()  const;
	color  cl()  const;
	flag   fl()  const;

	struct item
	{
		square sq1;
		square sq2;
		piece  pc;
		piece  vc;
		color  cl;
		flag   fl;

		item(move mv);
		piece promo_pc() const;
		bool promo() const;
		bool castling() const;
	};

	// determining convenient move properties

	piece promo_pc() const;
	bool castle() const;
	bool capture() const;
	bool promo() const;
	bool quiet() const;

	// converting the move into algebraic notation as specified by the UCI-protocol

	std::string algebraic() const;

	// defining castling squares

	constexpr static castle_sq rook_origin{ {{{H1, A1}}, {{H8, A8}}} };
	constexpr static castle_sq king_target{ {{{G1, C1}}, {{G8, C8}}} };
	constexpr static castle_sq rook_target{ {{{F1, D1}}, {{F8, D8}}} };
};

// defining some moves lists for convenience

using move_list    = std::array<move, lim::moves>;
using counter_list = std::array<std::array<std::array<move, 64>, 6>, 2>;

// killer move list

struct killer_list
{
	std::array<move, 2> mv;
	void update(move& quiet_mv);
};

// managing move variations

struct move_var
{
	std::array<move, lim::dt> mv{};
	key64 pos_key{};
	int cnt{};
	depth dt{};
	score sc{};
	bool tb_root{};
};

// search stack contains mostly various move lists

struct sstack
{
	depth dt{};
	score sc{ score::NONE };
	move  mv{};
	std::array<std::array<int, 64>, 6>* cont_mv{};
	move singular_mv{};
	killer_list killer{};
	struct null_move { bit64 ep{}; square sq{}; } null_mv{};
	move_list quiet_mv{};
	move_list capture_mv{};
	bool pruning{ true };
	int fail_high_cnt{};
};