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
	bool push_to_7th() const;

	// converting the move into algebraic notation as specified by the UCI-protocol

	std::string algebraic() const;

	// defining castling squares

	constexpr static castle_sq rook_origin{ {{{h1, a1}}, {{h8, a8}}} };
	constexpr static castle_sq king_target{ {{{g1, c1}}, {{g8, c8}}} };
	constexpr static castle_sq rook_target{ {{{f1, d1}}, {{f8, d8}}} };
};

// defining some moves lists for convenience

using move_list    = std::array<move, lim::moves>;
using counter_list = std::array<std::array<std::array<move, 64>, 6>, 2>;
using killer_list  = std::array<move, 2>;

// managing move variations

struct move_var
{
	std::array<move, lim::dt> mv{};
	int cnt{};
	depth dt{};
	depth seldt{};
	depth get_seldt();
	score sc{};
	bool wrong{};
};