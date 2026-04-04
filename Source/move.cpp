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


#include <string>

#include "main.h"
#include "types.h"
#include "uci.h"
#include "move.h"

static_assert(sizeof(move) == 4);

bool move::verify_move(square sq1, square sq2, piece pc, piece vc, color cl, flag fl)
{
	// checking the move for sanity

	return type::sq(sq1) && type::sq(sq2) && type::pc(pc)
		&& ((type::pc(vc) && vc != KING) || vc == NO_PIECE)
		&& type::cl(cl) && type::fl(fl);
}

move::move(square sq1, square sq2, piece pc, piece vc, color cl, flag fl)
{
	// compressing all move items into 22 bits

	verify(verify_move(sq1, sq2, pc, vc, cl, fl));
	mv = uint32(sq1 | (sq2 << 6) | (pc << 12) | (vc << 15) | (cl << 18) | (fl << 19));
}

uint32 move::raw() const
{
	return mv;
}

void move::set_sq2(square sq)
{
	// corrupting the move by setting a new to-square

	verify(type::sq(sq));
	mv = (mv & 0xfffff03fUL) | (sq << 6);
}

move::operator bool() const
{
	return raw() != 0L;
}

bool move::operator==(const move& m) const
{
	return raw() == m.raw();
}

bool move::operator!=(const move& m) const
{
	return raw() != m.raw();
}

square move::sq1() const
{
	return square((mv >> 0) & 63);
}

square move::sq2() const
{
	return square((mv >> 6) & 63);
}

piece move::pc() const
{
	return piece((mv >> 12) & 7);
}

piece move::vc() const
{
	return piece((mv >> 15) & 7);
}

color move::cl() const
{
	return color((mv >> 18) & 1);
}

flag move::fl() const
{
	return flag((mv >> 19) & 7);
}

move::item::item(move mv) :
	sq1{ mv.sq1() },
	sq2{ mv.sq2() },
	pc { mv.pc()  },
	vc { mv.vc()  },
	cl { mv.cl()  },
	fl { mv.fl()  }
{
	verify(verify_move(sq1, sq2, pc, vc, cl, fl));
}

piece move::item::promo_pc() const
{
    verify(fl >= PROMO_KNIGHT && fl <= PROMO_QUEEN);
	return piece(fl - 3);
}

bool move::item::promo() const
{
    return fl >= PROMO_KNIGHT;
}

bool move::item::castling() const
{
    return fl == CASTLE_EAST || fl == CASTLE_WEST;
}

piece move::promo_pc() const
{
	return piece(fl() - 3);
}

bool move::castle() const
{
    flag fl{ this->fl() };
    return fl == CASTLE_EAST || fl == CASTLE_WEST;
}

bool move::capture() const
{
	return vc() != NO_PIECE;
}

bool move::promo() const
{
    return fl() >= PROMO_KNIGHT;
}

bool move::quiet() const
{
	return vc() == NO_PIECE && !promo();
}

std::string move::algebraic() const
{
	// converting the move into algebraic notation, e.g. 'e2e4'
	// castling moves and promotions require special care

	if (!(*this)) return "0000";
	item m{ *this };

	if (!uci::chess960 && m.castling())
		m.sq2 = king_target[m.cl][m.fl];

	std::string coordinate{ type::sq_of(m.sq1) + type::sq_of(m.sq2) };
	if (m.promo())
		coordinate += "nbrq"[m.fl - PROMO_KNIGHT];

	return coordinate;
}

void killer_list::update(move& quiet_mv)
{
	// updating killer-moves if a quiet move fails high

	if (quiet_mv != mv[0])
	{
		mv[1] = mv[0];
		mv[0] = quiet_mv;
	}
}