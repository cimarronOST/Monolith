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


#include "uci.h"
#include "move.h"

static_assert(sizeof(move) == 4);

bool move::verify_move(square sq1, square sq2, piece pc, piece vc, color cl, flag fl)
{
	// checking the move for sanity

	return type::sq(sq1) && type::sq(sq2) && type::pc(pc)
		&& ((type::pc(vc) && vc != king) || vc == no_piece)
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
	verify(fl >= promo_knight && fl <= promo_queen);
	return piece(fl - 3);
}

bool move::item::promo() const
{
	return fl >= promo_knight;
}

bool move::item::castling() const
{
	return fl == castle_east || fl == castle_west;
}

piece move::promo_pc() const
{
	return piece(fl() - 3);
}

bool move::castle() const
{
	flag fl{ this->fl() };
	return fl == castle_east || fl == castle_west;
}

bool move::capture() const
{
	return vc() != no_piece;
}

bool move::promo() const
{
	return fl() >= promo_knight;
}

bool move::quiet() const
{
	return vc() == no_piece && !promo();
}

bool move::push_to_7th() const
{
	return pc() == pawn && type::rel_rk_of(type::rk_of(sq2()), cl()) == rank_7;
}

std::string move::algebraic() const
{
	// converting the move into algebraic notation, e.g. 'e2e4'
	// castling moves and promotions require special care

	if (!(*this)) return "0000";
	item mv{ *this };

	if (!uci::chess960 && mv.castling())
		mv.sq2 = king_target[mv.cl][mv.fl];

	std::string coordinate{ type::sq_of(mv.sq1) + type::sq_of(mv.sq2) };
	if (mv.promo())
		coordinate += "nbrq"[mv.fl - promo_knight];

	return coordinate;
}

depth move_var::get_seldt()
{
	// retrieving the selective depth, i.e. the highest reached depth

	depth new_dt{ seldt };
	for ( ; mv[new_dt]; ++new_dt);
	seldt = std::max(new_dt, std::max(dt, 1));
	return seldt;
}