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


#include "attack.h"
#include "bit.h"
#include "eval.h"

std::array<std::array<int, 6>, 2> eval::piece_value
{ {
	{{   71, 348, 371, 502, 1077, 0 }},
	{{  100, 342, 368, 618, 1206, 0 }}
} };

std::array<int, 6> eval::phase_value{ { 0, 2, 2, 3, 9 } };
std::array<int, 6> eval::complexity { { 6, 2, 17, 50, -33, -55 } };

// piece weights

std::array<int, 2> eval::bishop_pair       { {  28, 56 } };
std::array<int, 2> eval::bishop_color_pawns{ {  -6, -4 } };
std::array<int, 2> eval::bishop_trapped    { { -11, -5 } };
std::array<int, 2> eval::knight_outpost    { {  10,  6 } };
std::array<int, 4> eval::knight_distance_kings{ { -18, -12, -22, -69 } };

std::array<int, 2> eval::major_on_7th  { { -12, 27 } };
std::array<int, 2> eval::rook_open_file{ {  15,  5 } };

// threat weights

std::array<int, 2> eval::threat_pawn { {  11, -17 } };
std::array<int, 2> eval::threat_minor{ { -18, -20 } };
std::array<int, 2> eval::threat_rook { { -36,  -3 } };
std::array<int, 2> eval::threat_queen_by_minor{ { -19, -28 } };
std::array<int, 2> eval::threat_queen_by_rook { { -38, -38 } };
std::array<int, 2> eval::threat_piece_by_pawn { { -42,  -1 } };

// king safety weights

int eval::weak_king_sq{ 19 };
std::array<int,  5> eval::threat_king_by_check{ { 0, 48, 32, 81, 37 } };
std::array<int,  5> eval::threat_king_weight  { { 0,  4,  3,  4,  4 } };
std::array<int, 60> eval::threat_king
{ {
		  0,   0,   0,   0,   0,   0,   0,  -1,  -5,   1,
		  6,  13,   8,  28,  32,  35,  27,  57,  52,  60,
		 58,  76,  81, 112,  79, 114, 144, 117, 128, 155,
		152, 190, 179, 228, 200, 211, 220, 178, 303, 257,
		289, 331, 288, 293, 299, 301, 303, 469, 355, 397,
		436, 462, 298, 605, 617, 479, 467, 491, 926, 954
 } };

// pawn weights

std::array<int, 2> eval::isolated{ { -7, -10 } };
std::array<int, 2> eval::backward{ { -5,  -4 } };

std::array<std::array<std::array<int, 8>, 2>, 2> eval::connected
{ {
	{{
		{{  0,  3, 15, 16, 22, 34, 150,  0 }},
		{{  0, -2, 10,  9, 15, 53,  21,  0 }}
	}}
} };

std::array<int, 2> eval::king_distance_cl  { { -6, -8 } };
std::array<int, 2> eval::king_distance_cl_x{ {  0, 24 } };

std::array<std::array<int, 8>, 2> eval::passed_rank{ { {{ 0, -25, -22,  18,  72, 161, 289,   0 }} } };
std::array<std::array<int, 8>, 2> eval::shield_rank{ { {{ 0,   0,  -3, -11, -11,   6,   7, -27 }} } };
std::array<std::array<int, 8>, 2> eval::storm_rank { { {{ 0,   0,   0,   0,   9,   7,   0,   0 }} } };

// mobility weights

std::array<std::array<int, 9>, 2> eval::knight_mobility
{ {
	{{ -61, -25, -9,  0,  8, 13, 22, 31, 40 }},
	{{ -57, -26,  5, 18, 22, 29, 31, 29, 28 }}
} };
std::array<std::array<int, 14>, 2> eval::bishop_mobility
{ {
	{{ -44,  -5,  9, 20, 27, 34, 37, 40, 40, 43, 45, 48, 70, 86 }},
	{{ -36, -35, -8, 10, 21, 28, 32, 36, 42, 42, 40, 35, 37, 24 }}
} };
std::array<std::array<int, 15>, 2> eval::rook_mobility
{ {
	{{  -62, -49,  -4, 7,  6,  4,  5,  7, 11, 14, 16, 19, 18, 24, 16 }},
	{{ -108, -55, -13, 1, 13, 22, 29, 31, 33, 37, 43, 45, 50, 47, 55 }}
} };
std::array<std::array<int, 28>, 2> eval::queen_mobility
{ {
	{{ -253, -168, -75, -36,   -7,  -5,   0,   2,  8, 8, 13, 15, 16, 17, 17, 20, 16, 18,  7, 17, 19, 18, 23,  3, 31, 76, 44, 139 }},
	{{ -174, -103, -23, -49, -103, -54, -33, -12, -7, 8,  8, 12, 21, 25, 32, 36, 48, 45, 57, 53, 56, 51, 54, 81, 73, 44, 20,  45 }}
} };

// piece square tables

std::array<std::array<std::array<int, 64>, 2>, 2> eval::pawn_psq
{ {
	{{
		{{
				  0,   0,   0,   0,   0,   0,   0,   0,
				-76, -43,   2,  36,  36,   2, -43, -76,
				  0,   7,  38,  25,  25,  38,   7,   0,
				 -8,  -5,  -2,  10,  10,  -2,  -5,  -8,
				 -4,  -7,   4,  11,  11,   4,  -7,  -4,
				-12,  -6,  -9,  -2,  -2,  -9,  -6, -12,
				 -5,   6,   7,   0,   0,   7,   6,  -5,
				  0,   0,   0,   0,   0,   0,   0,   0
		}},
		{{
				  0,   0,   0,   0,   0,   0,   0,   0,
				  7,  -3, -24, -63, -63, -24,  -3,   7,
				 20,  16,  -5, -34, -34,  -5,  16,  20,
				  8,   3,  -2, -11, -11,  -2,   3,   8,
				 -3,  -2,  -5,  -5,  -5,  -5,  -2,  -3,
				 -7, -11,  -7,  -1,  -1,  -7, -11,  -7,
				 -1,  -1,   5,  10,  10,   5,  -1,  -1,
				  0,   0,   0,   0,   0,   0,   0,   0
		}}
	}}
} };
std::array<std::array<std::array<int, 64>, 2>, 2> eval::knight_psq
{ {
	{{
		{{
			   -141, -89, -99, -62, -62, -99, -89,-141,
				-32, -25,  27,   0,   0,  27, -25, -32,
				 -1,  23,  31,  24,  24,  31,  23,  -1,
				 24,  15,  28,  23,  23,  28,  15,  24,
				 12,  16,  14,  11,  11,  14,  16,  12,
				 -1,   8,  12,   9,   9,  12,   8,  -1,
				 -2,   0,   2,   5,   5,   2,   0,  -2,
				-31, -13,  -9,  -9,  -9,  -9, -13, -31
		}},
		{{
				-47, -14,  -4,   0,   0,  -4, -14, -47,
				-18,  15,   1,   4,   4,   1,  15, -18,
				 -5,   8,  16,  13,  13,  16,   8,  -5,
				  5,  18,  19,  21,  21,  19,  18,   5,
				 12,  11,  19,  27,  27,  19,  11,  12,
				  4,  12,  12,  22,  22,  12,  12,   4,
				 17,  11,   4,   9,   9,   4,  11,  17,
				  4,   4,   1,   7,   7,   1,   4,   4
		}}
	}}
} };
std::array<std::array<std::array<int, 64>, 2>, 2> eval::bishop_psq
{ {
	{{
		{{
				-73, -81, -67,-115,-115, -67, -81, -73,
				-50, -49, -32, -42, -42, -32, -49, -50,
				  1,   1,  11,   5,   5,  11,   1,   1,
				-31,  -5,  -3,  13,  13,  -3,  -5, -31,
				 -1, -19,  -5,   9,   9,  -5, -19,  -1,
				 -4,  10,   4,   4,   4,   4,  10,  -4,
				 14,  15,  14,  -4,  -4,  14,  15,  14,
				 11,   9,  -8,  -2,  -2,  -8,   9,  11
		}},
		{{
				 -4,   8,   4,  15,  15,   4,   8,  -4,
				 -7,  -4,   0,   3,   3,   0,  -4,  -7,
				 -6,   0,   2,  -5,  -5,   2,   0,  -6,
				  6,   8,   6,  11,  11,   6,   8,   6,
				 -6,  -1,   8,   8,   8,   8,  -1,  -6,
				 -8,   4,   4,   9,   9,   4,   4,  -8,
				 -6,  -6,  -8,   1,   1,  -8,  -6,  -6,
				 -3, -11,   5,  -1,  -1,   5, -11,  -3
		}}
	}}
} };
std::array<std::array<std::array<int, 64>, 2>, 2> eval::rook_psq
{ {
	{{
		{{
				 45,  13,  40,  15,  15,  40,  13,  45,
				 11,  -1,  16,  19,  19,  16,  -1,  11,
				 -5,  21,   5,  14,  14,   5,  21,  -5,
				-15,  -5,  -2,  -3,  -3,  -2,  -5, -15,
				-15,  -7, -21, -14, -14, -21,  -7, -15,
				-16,   4,  -7,  -9,  -9,  -7,   4, -16,
				-16,  -7,  -5,   0,   0,  -5,  -7, -16,
				 -8,  -3,   0,   4,   4,   0,  -3,  -8
		}},
		{{
				  1,  15,  26,  27,  27,  26,  15,   1,
				 -5,  16,  15,   8,   8,  15,  16,  -5,
				 11,  16,  24,  14,  14,  24,  16,  11,
				 15,  20,  17,  11,  11,  17,  20,  15,
				  7,   5,  13,   9,   9,  13,   5,   7,
				 -1,  -6,  -2,   0,   0,  -2,  -6,  -1,
				 -2,  -5,  -6,  -3,  -3,  -6,  -5,  -2,
				 -1,  -7,  -3, -10, -10,  -3,  -7,  -1
		}}
	}}
} };
std::array<std::array<std::array<int, 64>, 2>, 2> eval::queen_psq
{ {
	{{
		{{
				-11,   0,  45,  31,  31,  45,   0, -11,
				 25, -23,  -4, -23, -23,  -4, -23,  25,
				 36,  28,  -1,   1,   1,  -1,  28,  36,
				  7,   3,  -7, -13, -13,  -7,   3,   7,
				  8,   9,   3,   1,   1,   3,   9,   8,
				 15,  18,   8,   5,   5,   8,  18,  15,
				 17,  19,  18,  18,  18,  18,  19,  17,
				 20,   8,   4,  13,  13,   4,   8,  20
		}},
		{{
				-11,   2,  28,  44,  44,  28,   2, -11,
				-27,  27,  48,  62,  62,  48,  27, -27,
				  6,  21,  66,  66,  66,  66,  21,   6,
				 38,  59,  54,  70,  70,  54,  59,  38,
				 35,  35,  32,  48,  48,  32,  35,  35,
				  0,  13,  30,  21,  21,  30,  13,   0,
				-15, -18, -15,   0,   0, -15, -18, -15,
				-38, -10,  -4, -16, -16,  -4, -10, -38
		}}
	}}
} };
std::array<std::array<std::array<int, 64>, 2>, 2> eval::king_psq
{ {
	{{
		{{
				-58,  75,  -4,  17,  17,  -4,  75, -58,
				-44,  40,  62,  29,  29,  62,  40, -44,
				-72,  30,  24, -13, -13,  24,  30, -72,
			   -132, -48, -67, -76, -76, -67, -48,-132,
			   -158, -75, -63, -70, -70, -63, -75,-158,
				-59,  -2, -21, -26, -26, -21,  -2, -59,
				 25,  37,  16,  -9,  -9,  16,  37,  25,
				 28,  49,  30,  20,  20,  30,  49,  28,
		}},
		{{
			   -190, -80, -26, -21, -21, -26, -80,-190,
			    -21,  21,  34,  24,  24,  34,  21, -21,
				 -5,  35,  38,  43,  43,  38,  35,  -5,
				 -5,  25,  42,  49,  49,  42,  25,  -5,
				 -8,  11,  23,  33,  33,  23,  11,  -8,
				-22,  -9,   1,  12,  12,   1,  -9, -22,
				-48, -33, -15,  -6,  -6, -15, -33, -48,
				-86, -58, -35, -35, -35, -35, -58, -86
		}}
	}}
} };

namespace
{
	int sum_pieces(const board& pos, color cl)
	{
		// summing up phase values of all pieces on the board

		int sum{};
		for (piece pc : { knight, bishop, rook, queen })
			sum += bit::popcnt(pos.pieces[pc] & pos.side[cl]) * eval::phase_value[pc];
		return sum;
	}

	int interpolate(int sc_mg, int sc_eg, int phase)
	{
		// interpolating the mid- & end-game scores

		static const int max_weight
		{
			 16 * eval::phase_value[pawn]
			+ 4 * eval::phase_value[knight]
			+ 4 * eval::phase_value[bishop]
			+ 4 * eval::phase_value[rook]
			+ 2 * eval::phase_value[queen]
		};
		verify(phase >= 0);
		int weight{ std::min(phase, max_weight) };
		return (sc_mg * weight + sc_eg * (max_weight - weight)) / max_weight;
	}

	struct king_pressure
	{
		// keeping track of attacks near the enemy king which increase the pressure on the king
		// the collected scores are used for king safety evaluation

		bit64 king_zone;
		int cnt{};
		int sum{};

		king_pressure(const board& pos, color cl) : king_zone(bit::king_zone[cl][pos.sq_king[cl]]) {}
		void add(piece pc, bit64& targets)
		{
			// adding attacks

			verify(pc <= queen);
			if (bit64 king_attack{ targets & king_zone }; king_attack)
			{
				cnt += 1;
				sum += eval::threat_king_weight[pc] * bit::popcnt(king_attack);
			}
		}
	};
}

namespace
{
	// assessing material constellations

	bool opposite_bishops(const board& pos)
	{
		return (pos.pieces[bishop] | pos.pieces[pawn] | pos.pieces[king]) == pos.side[both]
			&& bit::popcnt(pos.pieces[bishop] & pos.side[white]) == 1
			&& bit::popcnt(pos.pieces[bishop] & pos.side[black]) == 1
			&& (pos.pieces[bishop] & bit::sq_white)
			&& (pos.pieces[bishop] & bit::sq_black);
	}

	bool pawn_passed(const board& pos, square sq, color cl, color cl_x)
	{
		verify(pos.side[cl] & bit::set(sq));
		verify(cl == (cl_x ^ 1));

		return !(bit::fl_in_front[cl][sq] & pos.pieces[pawn]) && !(bit::front_span[cl][sq] & pos.pieces[pawn] & pos.side[cl_x]);
	}

	int draw_scale(const board& pos, int sc)
	{
		// scaling the score for drawish positions

		color winning{ sc > 0 ? white : black };

		if (!(pos.side[winning] & pos.pieces[pawn])
			&& sum_pieces(pos, winning) - sum_pieces(pos, winning ^ 1) <= eval::phase_value[knight])
			sc /= 4;

		if (opposite_bishops(pos))
			sc = sc * 3 / 4;

		return sc;
	}

	bool obvious_draw(const board& pos)
	{
		// recognizing positions with insufficient mating material
		// KvK, KNvK, KBvK, KB*vKB*, KNNvK

		if (pos.lone_bishops() && (!(bit::sq_white & pos.pieces[bishop]) || !(bit::sq_black & pos.pieces[bishop])))
			return true;

		if (pos.lone_knights() && (!(pos.pieces[knight] & pos.side[white]) || !(pos.pieces[knight] & pos.side[black])))
			return true;

		return false;
	}
}

namespace eval
{
	void passed_pawns(const board& pos, eval_score& sum, all_attacks& att_by_1, kingpawn_hash::hash& entry, color cl)
	{
		// evaluating passed pawns (~157 Elo)
		// this is the only pawn-related evaluation that cannot be stored in the king-pawn hash table

		color  cl_x{ cl ^ 1 };
		bit64  passed{ entry.passed[cl] };
		while (passed)
		{
			square sq{ bit::scan(passed) };
			rank   rk{ type::rk_of(sq) };

			// assigning a big bonus for a normal passed pawn

			int mg_bonus{ passed_rank[cl][rk] };
			int eg_bonus{ passed_rank[cl][rk] };

			// adding a king distance bonus

			square sq_stop{ sq + shift::push1x[0] * (cl == white ? 1 : -1) };
			int dist_cl   { type::sq_distance(sq_stop, pos.sq_king[cl]) };
			int dist_cl_x { type::sq_distance(sq_stop, pos.sq_king[cl_x]) };

			mg_bonus += king_distance_cl[mg] * dist_cl + king_distance_cl_x[mg] * dist_cl_x;
			eg_bonus += king_distance_cl[eg] * dist_cl + king_distance_cl_x[eg] * dist_cl_x;

			// adding X-ray attacks by major pieces behind the passed pawn to the attack- and defense-table

			bit64 attacked{ att_by_1[cl_x] };
			bit64 defended{ att_by_1[cl] };

			bit64 major{ bit::fl_in_front[cl_x][sq] & (pos.pieces[rook] | pos.pieces[queen]) };
			if (major && (major & attack::by_slider<rook>(sq, pos.side[both])))
			{
				if (major & pos.side[cl])
					defended |= bit::fl_in_front[cl][sq];
				else
					attacked |= bit::fl_in_front[cl][sq];
			}

			// penalizing if the path to promotion is blocked, attacked or undefended

			if (bit64 blocked_path{ bit::fl_in_front[cl][sq] & (pos.side[cl_x] | (attacked & ~defended)) }; blocked_path)
			{
				int blocked{ bit::popcnt(blocked_path) };
				verify(blocked <= 6);

				mg_bonus /= blocked + 2;
				eg_bonus /= blocked + 1;
			}

			sum[mg][cl] += mg_bonus;
			sum[eg][cl] += eg_bonus;
			passed &= passed - 1;
		}
	}

	void pawns(const board& pos, kingpawn_hash::hash& entry)
	{
		// evaluating everything pawn-related that can be stored in the king-pawn hash table

		for (color cl : {white, black})
		{
			color cl_x{ cl ^ 1 };
			file  fl_king{ type::fl_of(pos.sq_king[cl]) };
			entry.attack[cl_x] = attack::by_pawns(pos.pieces[pawn] & pos.side[cl_x], cl_x);

			bit64 pawns{ pos.pieces[pawn] & pos.side[cl] };
			bit64 pawns_cl{ pawns };
			bit64 pawns_cl_x{ pos.pieces[pawn] & pos.side[cl_x] };

			// pawn formations relative to the king

			for (file fl : { file(fl_king + 1), fl_king, file(fl_king - 1) })
			{
				if (!type::fl(fl)) continue;
				square sq{ type::sq_of(type::rel_rk_of(rank_1, cl), fl) };

				// pawn shield (~41 Elo)

				bit64 sq_pawn{ attack::by_slider<rook>(sq, pawns_cl) & bit::file[fl] & pawns_cl };
				verify(bit::popcnt(sq_pawn) <= 1);

				int sc{ sq_pawn ? shield_rank[cl][type::rk_of(bit::scan(sq_pawn))] : shield_rank[white][rank_8] };
				entry.score[mg][cl] += (fl == fl_king) ? sc * 2 : sc;

				// pawn storm (~0 Elo)

				sq_pawn = attack::by_slider<rook>(sq, pawns_cl_x) & pawns_cl_x & bit::file[fl];
				verify(bit::popcnt(sq_pawn) <= 1);

				sc = { sq_pawn ? storm_rank[cl_x][type::rk_of(bit::scan(sq_pawn))] : 0 };
				entry.score[mg][cl_x] += (bit::shift(sq_pawn, shift::push1x[cl_x]) & pawns_cl) ? sc : sc * 2;
			}

			while (pawns)
			{
				square sq{ bit::scan(pawns) };
				bit64 sq_bit{ bit::set(sq) };
				bit64 sq_stop{ bit::shift(sq_bit, shift::push1x[cl]) };
				file fl{ type::fl_of(sq) };
				rank rk{ type::rk_of(sq) };

				// PSQT

				entry.score[mg][cl] += pawn_psq[cl_x][mg][sq] + piece_value[mg][pawn];
				entry.score[eg][cl] += pawn_psq[cl_x][eg][sq] + piece_value[eg][pawn];

				// finding passed pawn

				if (pawn_passed(pos, sq, cl, cl_x))
					entry.passed[cl] |= sq_bit;

				// penalizing isolated pawn (~12 Elo)

				if (!(bit::fl_adjacent[fl] & pawns_cl))
				{
					entry.score[mg][cl] += isolated[mg];
					entry.score[eg][cl] += isolated[eg];
				}

				// penalizing backward pawn (~7 Elo)

				if ((entry.attack[cl_x] & sq_stop) && !((bit::front_span[cl_x][bit::scan(sq_stop)] & pawns_cl) ^ sq_bit))
				{
					entry.score[mg][cl] += backward[mg];
					entry.score[eg][cl] += backward[eg];
				}

				// rewarding connected pawn (~40 Elo)

				else if (bit::connected[cl][sq] & pawns_cl)
				{
					entry.score[mg][cl] += connected[cl][mg][rk];
					entry.score[eg][cl] += connected[cl][eg][rk];
				}

				pawns &= pawns - 1;
			}
		}
	}

	void tactics(const board& pos, eval::eval_score& sum, const attack_list& att, const all_attacks& att_by_1, color cl)
	{
		// penalizing tactical threats (~12 Elo)

		color cl_x{ cl ^ 1 };
		bit64 minors{ pos.pieces[knight] | pos.pieces[bishop] };
		bit64 minor_attacks{ att[cl_x][knight] | att[cl_x][bishop] };
		int threat{};

		// penalizing threats against unsupported pawns

		threat = bit::popcnt(pos.pieces[pawn] & pos.side[cl] & (att_by_1[cl_x] & ~att_by_1[cl]));
		sum[mg][cl] += threat * threat_pawn[mg];
		sum[eg][cl] += threat * threat_pawn[eg];

		// penalizing threats against minor pieces

		threat = bit::popcnt(minors & pos.side[cl] & minor_attacks);
		sum[mg][cl] += threat * threat_minor[mg];
		sum[eg][cl] += threat * threat_minor[eg];

		// penalizing threats against rooks

		threat = bit::popcnt(pos.pieces[rook] & pos.side[cl] & minor_attacks);
		sum[mg][cl] += threat * threat_rook[mg];
		sum[eg][cl] += threat * threat_rook[eg];

		// penalizing threats against all pieces from pawns

		threat = bit::popcnt((minors | pos.pieces[rook] | pos.pieces[queen]) & pos.side[cl] & att[cl_x][pawn]);
		sum[mg][cl] += threat * threat_piece_by_pawn[mg];
		sum[eg][cl] += threat * threat_piece_by_pawn[eg];

		// penalizing threats against queens from minor pieces

		threat = bit::popcnt(pos.pieces[queen] & pos.side[cl] & minor_attacks);
		sum[mg][cl] += threat * threat_queen_by_minor[mg];
		sum[eg][cl] += threat * threat_queen_by_minor[eg];

		// penalizing threats against queens from rooks

		threat = bit::popcnt(pos.pieces[queen] & pos.side[cl] & att[cl_x][rook]);
		sum[mg][cl] += threat * threat_queen_by_rook[mg];
		sum[eg][cl] += threat * threat_queen_by_rook[eg];
	}

	void king_safety(const board& pos, eval_score& sum, const attack_list& att, const all_attacks& att_by_1, const all_attacks& att_by_2,
		king_pressure& pressure, color cl)
	{
		// evaluating king safety threats

		color cl_x{ cl ^ 1 };
		bit64 weak_sq{  att_by_1[cl] & (~att_by_1[cl_x] | att[cl_x][queen] | att[cl_x][king]) & ~att_by_2[cl_x] };
		bit64 safe_sq{ (~att_by_1[cl_x] | (weak_sq & att_by_2[cl])) & ~pos.side[cl] };

		bit64 bishop_reach{ attack::by_slider<bishop>(pos.sq_king[cl_x], pos.side[both]) };
		bit64 rook_reach  { attack::by_slider<rook>  (pos.sq_king[cl_x], pos.side[both]) };

		// if the enemy king is attacked by at least 2 pieces, it is considered as threated

		if (pressure.cnt >= 2)
		{
			// the king threat score is consolidated by all attacks of the king zone (~35 Elo)
			// if no queen is involved in the attack, the score is halved

			int weight{ pressure.sum };
			weight /= (pos.pieces[queen] & pos.side[cl]) ? 1 : 2;
			sum[mg][cl_x] -= threat_king[std::min(weight, 59)];

			// weak squares around the king are an addition to the threat

			sum[mg][cl_x] -= bit::popcnt(pressure.king_zone &  weak_sq) * weak_king_sq;
		}

		// the enemy king is also considered threatened if a piece can give a safe check on the next move (~42 Elo)
		// safe check threat of knights

		int cnt{ bit::popcnt(bit::pc_attack[knight][pos.sq_king[cl_x]] & att[cl][knight] & safe_sq) };
		sum[mg][cl_x] -= threat_king_by_check[knight] * cnt;

		// safe check threat of bishops

		cnt = bit::popcnt(bishop_reach & att[cl][bishop] & safe_sq);
		sum[mg][cl_x] -= threat_king_by_check[bishop] * cnt;

		// safe check threat of rooks

		cnt = bit::popcnt(rook_reach & att[cl][rook] & safe_sq);
		sum[mg][cl_x] -= threat_king_by_check[rook] * cnt;

		// safe check threat of queens

		cnt = bit::popcnt((bishop_reach | rook_reach) & att[cl][queen] & safe_sq);
		sum[mg][cl_x] -= threat_king_by_check[queen] * cnt;
	}

	int initiative(const board &pos, int sc_eg, const kingpawn_hash::hash& entry)
	{
		// evaluating the initiative of the side that has the advantage (~0 Elo)
		// the computed score is applied as a correction
		// idea from Stockfish:
		// https://github.com/official-stockfish/Stockfish

		int outflanking{  std::abs(type::fl_of(pos.sq_king[white]) - type::fl_of(pos.sq_king[black]))
					    - std::abs(type::rk_of(pos.sq_king[white]) - type::rk_of(pos.sq_king[black])) };
		int passed_pawns{ bit::popcnt(entry.passed[white] | entry.passed[black]) };

		bool pawns_on_both_flanks{ (pos.pieces[pawn] & bit::half_east) && (pos.pieces[pawn] & bit::half_west) };
		bool almost_unwinnable{ outflanking < 0 && !passed_pawns && !pawns_on_both_flanks };

		int sc_complexity =
			+ complexity[0] * bit::popcnt(pos.pieces[pawn])
			+ complexity[1] * outflanking
			+ complexity[2] * pawns_on_both_flanks
			+ complexity[3] * pos.lone_pawns()
			+ complexity[4] * almost_unwinnable
			+ complexity[5];

		// if the score is positive, white has the advantage, otherwise black
		// the score is also not allowed to change sign after the correction

		return ((sc_eg > 0) - (sc_eg < 0)) * std::max(sc_complexity, -std::abs(sc_eg));
	}

	void pieces(const board& pos, eval_score& sum, attack_list& att, all_attacks& att_by_1, all_attacks& att_by_2,
		king_pressure& pressure, int& phase, color cl)
	{
		// evaluating all pieces except pawns
		// starting by initializing king pressure & finding all pins to restrict piece mobility

		color cl_x{ cl ^ 1 };
		bit64 pawns_cl  { pos.pieces[pawn] & pos.side[cl] };
		bit64 pawns_cl_x{ pos.pieces[pawn] & pos.side[cl_x] };

		// defining the mobility area

		bit64 blocked_pawns{ pawns_cl & bit::shift(pos.side[both], shift::push1x[cl_x]) };
		bit64 mobility_area{ ~(blocked_pawns | ((pos.pieces[king] | pos.pieces[queen]) & pos.side[cl]) | att[cl_x][pawn]) };

		// starting with the evaluation
		// knights

		bit64 pieces{ pos.side[cl] & pos.pieces[knight] };
		while (pieces)
		{
			square sq{ bit::scan(pieces) };

			sum[mg][cl] += knight_psq[cl_x][mg][sq] + piece_value[mg][knight];
			sum[eg][cl] += knight_psq[cl_x][eg][sq] + piece_value[eg][knight];

			// generating attacks

			bit64 targets{ bit::pc_attack[knight][sq] & mobility_area };
			att[cl][knight] |= targets;
			att_by_2[cl]    |= targets & att_by_1[cl];
			att_by_1[cl]    |= targets;
			pressure.add(knight, targets);

			// rewarding outposts (~6 Elo)

			if (bit::set(sq) & bit::outpost_zone[cl] & att[cl][pawn])
			{
				int weight{ 1 };

				if (!(bit::front_span[cl][sq] & ~bit::file[type::fl_of(sq)] & pawns_cl_x))
					weight += 3;

				sum[mg][cl] += knight_outpost[mg] * weight;
				sum[eg][cl] += knight_outpost[eg] * weight;
			}

			// mobility (~53 Elo)

			int pop{ bit::popcnt(targets) };
			sum[mg][cl] += knight_mobility[mg][pop];
			sum[eg][cl] += knight_mobility[eg][pop];

			// penalizing long distance to kings (~0 Elo)

			int king_distance{ std::min(type::sq_distance(sq, pos.sq_king[cl]), type::sq_distance(sq, pos.sq_king[cl_x])) };
			if (king_distance > 3)
				sum[mg][cl] += knight_distance_kings[king_distance - 4];

			phase += phase_value[knight];
			pieces &= pieces - 1;
		}

		// bishop

		pieces = pos.side[cl] & pos.pieces[bishop];
		while (pieces)
		{
			square sq{ bit::scan(pieces) };

			sum[mg][cl] += bishop_psq[cl_x][mg][sq] + piece_value[mg][bishop];
			sum[eg][cl] += bishop_psq[cl_x][eg][sq] + piece_value[eg][bishop];

			// generating attacks

			bit64 targets{ attack::by_slider<bishop>(sq, pos.side[both] ^ pos.pieces[queen]) & mobility_area };
			att[cl][bishop] |= targets;
			att_by_2[cl]    |= targets & att_by_1[cl];
			att_by_1[cl]    |= targets;
			pressure.add(bishop, targets);

			// bishop pair bonus (~16 Elo)

			if (pieces & (pieces - 1))
			{
				sum[mg][cl] += bishop_pair[mg];
				sum[eg][cl] += bishop_pair[eg];
			}

			// mobility (~82 Elo)

			int pop{ bit::popcnt(targets) };
			sum[mg][cl] += bishop_mobility[mg][pop];
			sum[eg][cl] += bishop_mobility[eg][pop];

			// penalty for pawns on same colored squares (~8 Elo)

			if (bit::set(sq) & bit::sq_white)
			{
				int pawns{ bit::popcnt(pawns_cl & bit::sq_white) };
				sum[mg][cl] += bishop_color_pawns[mg] * pawns;
				sum[eg][cl] += bishop_color_pawns[eg] * pawns;
			}
			else
			{
				verify(bit::set(sq) & bit::sq_black);
				int pawns{ bit::popcnt(pawns_cl & bit::sq_black) };
				sum[mg][cl] += bishop_color_pawns[mg] * pawns;
				sum[eg][cl] += bishop_color_pawns[eg] * pawns;
			}

			// penalty if the enemy half of the board cannot be reached (~19 Elo)

			if (!(targets & bit::board_half[cl_x]))
			{
				sum[mg][cl] += bishop_trapped[mg];
				sum[eg][cl] += bishop_trapped[eg];
			}

			phase += phase_value[bishop];
			pieces &= pieces - 1;
		}

		// rook

		pieces = pos.side[cl] & pos.pieces[rook];
		while (pieces)
		{
			square sq{ bit::scan(pieces) };

			sum[mg][cl] += rook_psq[cl_x][mg][sq] + piece_value[mg][rook];
			sum[eg][cl] += rook_psq[cl_x][eg][sq] + piece_value[eg][rook];

			// generating attacks

			bit64 targets{ attack::by_slider<rook>(sq, pos.side[both] & ~(pos.pieces[queen] | (pos.pieces[rook] & pos.side[cl]))) & mobility_area };
			att[cl][rook] |= targets;
			att_by_2[cl]  |= targets & att_by_1[cl];
			att_by_1[cl]  |= targets;
			pressure.add(rook, targets);

			// being on a open or semi-open file (~30 Elo)

			bit64 fl_rook{ bit::file[type::fl_of(sq)] };
			if (!(fl_rook & pawns_cl))
			{
				int weight{ 1 };

				if (!(fl_rook & pos.pieces[pawn]))
					weight += 1;

				if (fl_rook & pos.pieces[king] & pos.side[cl_x])
					weight += 1;

				sum[mg][cl] += rook_open_file[mg] * weight;
				sum[eg][cl] += rook_open_file[eg] * weight;
			}

			// being on the 7th rank (~19 Elo)

			if (type::rel_rk_of(type::rk_of(sq), cl) == rank_7)
			{
				if (bit::rank[type::rel_rk_of(rank_7, cl)] & pawns_cl_x
					|| bit::rank[type::rel_rk_of(rank_8, cl)] & pos.pieces[king] & pos.side[cl_x])
				{
					sum[mg][cl] += major_on_7th[mg];
					sum[eg][cl] += major_on_7th[eg];
				}
			}

			// mobility (~41 Elo)

			int pop{ bit::popcnt(targets) };
			sum[mg][cl] += rook_mobility[mg][pop];
			sum[eg][cl] += rook_mobility[eg][pop];

			phase += phase_value[rook];
			pieces &= pieces - 1;
		}

		// queen

		pieces = pos.side[cl] & pos.pieces[queen];
		while (pieces)
		{
			square sq{ bit::scan(pieces) };

			sum[mg][cl] += queen_psq[cl_x][mg][sq] + piece_value[mg][queen];
			sum[eg][cl] += queen_psq[cl_x][eg][sq] + piece_value[eg][queen];

			// generating attacks

			bit64 targets{ attack::by_slider<queen>(sq, pos.side[both]) & mobility_area };
			att[cl][queen] |= targets;
			att_by_2[cl]   |= targets & att_by_1[cl];
			att_by_1[cl]   |= targets;
			pressure.add(queen, targets);

			// being on the 7th rank (~3 Elo)

			if (type::rel_rk_of(type::rk_of(sq), cl) == rank_7)
			{
				if (bit::rank[type::rel_rk_of(rank_7, cl)] & pawns_cl_x
					|| bit::rank[type::rel_rk_of(rank_8, cl)] & pos.pieces[king] & pos.side[cl_x])
				{
					sum[mg][cl] += major_on_7th[mg];
					sum[eg][cl] += major_on_7th[eg];
				}
			}

			// mobility (~17 Elo)

			int count{ bit::popcnt(targets) };
			sum[mg][cl] += queen_mobility[mg][count];
			sum[eg][cl] += queen_mobility[eg][count];

			phase += phase_value[queen];
			pieces &= pieces - 1;
		}

		// king

		{
			sum[mg][cl] += king_psq[cl_x][mg][pos.sq_king[cl]] + piece_value[mg][king];
			sum[eg][cl] += king_psq[cl_x][eg][pos.sq_king[cl]] + piece_value[eg][king];

			// generating attacks

			bit64 targets{ bit::pc_attack[king][pos.sq_king[cl]] };
			att[cl][king] |= targets;
			att_by_2[cl]  |= targets & att_by_1[cl];
			att_by_1[cl]  |= targets;
		}
	}

	void evaluate(const board& pos, eval_score& sum, int& phase, kingpawn_hash::hash& entry)
	{
		// beginning with the evaluation of the position

		if (pos.pieces[pawn])
		{
			// creating a new table entry if nothing is found in the pawn hash table
			// the hash table speeds up the engine considerably (~40 Elo)
			// a table entry provides some basic pawn evaluation

			verify(pos.key_kingpawn);
			if (entry.key != pos.key_kingpawn)
			{
				entry = kingpawn_hash::hash{};
				pawns(pos, entry);
				entry.key = pos.key_kingpawn;
			}

			sum[mg][white] = entry.score[mg][white];
			sum[eg][white] = entry.score[eg][white];
			sum[mg][black] = entry.score[mg][black];
			sum[eg][black] = entry.score[eg][black];
		}

		// initializing attack tables

		std::array<king_pressure, 2> pressure{ { { pos, white }, { pos, black } } };
		attack_list att{ { {{ entry.attack[white] }}, {{ entry.attack[black] }} } };
		all_attacks att_by_1{ { entry.attack[white], entry.attack[black] } };
		all_attacks att_by_2{};

		// evaluating pieces

		pieces(pos, sum, att, att_by_1, att_by_2, pressure[black], phase, white);
		pieces(pos, sum, att, att_by_1, att_by_2, pressure[white], phase, black);

		// evaluating tactical threats against pieces & pawns

		tactics(pos, sum, att, att_by_1, white);
		tactics(pos, sum, att, att_by_1, black);

		// evaluating king safety threats

		king_safety(pos, sum, att, att_by_1, att_by_2, pressure[black], white);
		king_safety(pos, sum, att, att_by_1, att_by_2, pressure[white], black);

		// extending the pawn evaluation

		passed_pawns(pos, sum, att_by_1, entry, white);
		passed_pawns(pos, sum, att_by_1, entry, black);
	}
}

score eval::static_eval(const board& pos, kingpawn_hash& hash)
{
	// entry point of the evaluation chain
	// filtering out obviously drawn positions with insufficient mating material first

	if (obvious_draw(pos))
		return score::draw;

	// initializing & probing the pawn hash table

	int phase{};
	eval_score sum{};
	kingpawn_hash::hash new_entry{};
	kingpawn_hash::hash& entry{ !hash.table.empty() && pos.pieces[pawn] ? hash.table[pos.key_kingpawn & kingpawn_hash::mask] : new_entry };

	// evaluating the position

	evaluate(pos, sum, phase, entry);

	// adding initiative correction before interpolating the scores

	int sc_mg{ sum[mg][white] - sum[mg][black] };
	int sc_eg{ sum[eg][white] - sum[eg][black] };
	sc_eg += initiative(pos, sc_eg, entry);

	int sc{ interpolate(sc_mg, sc_eg, phase) };
	verify(std::abs(sc) < int(score::longest_mate));

	// scaling drawish positions (~17 Elo) & adjusting the sign relative to the side to move

	sc = draw_scale(pos, sc);
	return score(sc * (pos.cl == white ? 1 : -1));
}

void eval::mirror_tables()
{
	// mirroring various tables for black

	for (gamestage st : {mg, eg})
	{
		for (square sq{ h1 }; sq <= a8; sq += 1)
		{
			pawn_psq[black][st][a8 - sq] = pawn_psq[white][st][sq];
			knight_psq[black][st][a8 - sq] = knight_psq[white][st][sq];
			bishop_psq[black][st][a8 - sq] = bishop_psq[white][st][sq];
			rook_psq[black][st][a8 - sq] = rook_psq[white][st][sq];
			queen_psq[black][st][a8 - sq] = queen_psq[white][st][sq];
			king_psq[black][st][a8 - sq] = king_psq[white][st][sq];
		}
	}

	for (int rk{ rank_1 }; rk <= rank_8; ++rk)
	{
		passed_rank[black][rk] = passed_rank[white][rank_8 - rk];
		shield_rank[black][rk] = shield_rank[white][rank_8 - rk];
		storm_rank[black][rk] = storm_rank[white][rank_8 - rk];

		connected[black][mg][rk] = connected[white][mg][rank_8 - rk];
		connected[black][eg][rk] = connected[white][eg][rank_8 - rk];
	}
}
