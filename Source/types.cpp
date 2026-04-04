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
#include <algorithm>
#include <cmath>

#include "main.h"
#include "types.h"

file type::fl_of(square sq)
{
	// determining the file of the square

	verify(type::sq(sq));
	return file(sq & 7);
}

rank type::rk_of(square sq)
{
	// determining the rank of the square

	verify(type::sq(sq));
	return rank(sq >> 3);
}

rank type::rk_of(rank rk, color cl)
{
	// determining the rank of a piece relative to its color

	verify(type::rk(rk) && type::cl(cl));
	return rank(rk ^ (7 * cl));
}

square type::sq_of(rank rk, file fl)
{
	// calculating the square when given rank and file

	verify(type::rk(rk) && type::fl(fl));
	return square((rk << 3) + fl);
}

square type::sq_of(std::string sq)
{
	// converting coordinate strings to the internal square-type

	verify(sq.size()  == 2);
	verify(sq.front() >= 'a' && sq.front() <= 'h');
	verify(sq.back()  >= '1' && sq.back()  <= '8');

	return sq_of(rank(sq.back() - '1'), file('h' - sq.front()));
}

std::string type::sq_of(square sq)
{
	// converting a square into a coordinate string

	verify(type::sq(sq));
	std::string sq_str{};
	sq_str += 'h' - char(fl_of(sq));
	sq_str += '1' + char(rk_of(sq));
	return sq_str;
}

square type::sq_flip(square sq)
{
	// mirroring the square around a vertical axis

	verify(type::sq(sq));
	return square(sq ^ 7);
}

int type::sq_distance(square sq1, square sq2)
{
	// determining the shortest distance between two squares

	verify(type::sq(sq1));
	verify(type::sq(sq2));
	return std::max(std::abs(fl_of(sq1) - fl_of(sq2)), std::abs(rk_of(sq1) - rk_of(sq2)));
}
