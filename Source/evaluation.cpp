/*
  Monolith 0.3  Copyright (C) 2017 Jonas Mayr

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


#include "magic.h"
#include "attack.h"
#include "movegen.h"
#include "bitboard.h"
#include "evaluation.h"

namespace
{
	uint64 front_span[2][64]{ };

	const uint32 seventh_rank[]{ R7, R2 };
	const int push[]{ 8, 56, -8 };
	const int negate[]{ 1, -1 };

	const int max_weight{ 42 };

	static_assert(WHITE == 0, "index");
	static_assert(BLACK == 1, "index");
	static_assert(MG == 0, "index");
	static_assert(EG == 1, "index");

	// finding pins for mobility evaluation
	// similar to the pinning function in movegen

	struct pinned
	{
		uint64 moves[64];
		uint32 idx[8];
		int cnt;
	} pin;

	void pin_down(pos &board, int turn)
	{
		pin.cnt = 0;
		int king_sq{ board.king_sq[turn] };
		int not_turn{ turn ^ 1 };

		uint64 fr_king = board.pieces[KINGS] & board.side[turn];

		uint64 all_att{ movegen::slide_ray[ROOK][king_sq] & board.side[not_turn] & (board.pieces[ROOKS] | board.pieces[QUEENS]) };
		all_att |= movegen::slide_ray[BISHOP][king_sq] & board.side[not_turn] & (board.pieces[BISHOPS] | board.pieces[QUEENS]);

		while (all_att)
		{
			// generating rays centered on the king square

			uint64 ray_to_att{ attack::by_slider(ROOK, king_sq, all_att) };
			ray_to_att |= attack::by_slider(BISHOP, king_sq, all_att);

			uint64 att{ 1ULL << bb::bitscan(all_att) };

			if (!(att & ray_to_att))
			{
				all_att &= all_att - 1;
				continue;
			}

			// creating final ray from king to attacker

			assert(fr_king);

			uint64 x_ray{ 0 };
			for (int dir{ 0 }; dir < 8; ++dir)
			{
				auto flood{ fr_king };
				for (; !(flood & magic::ray[dir].boarder); flood |= shift(flood, magic::ray[dir].shift));

				if (flood & att)
				{
					x_ray = flood & ray_to_att;
					break;
				}
			}

			assert(x_ray & att);
			assert(!(x_ray & fr_king));

			// pinning all legal moves

			if ((x_ray & board.side[turn]) && bb::popcnt(x_ray & board.side[BOTH]) == 2)
			{
				assert(bb::popcnt(x_ray & board.side[turn]) == 1);

				auto sq{ bb::bitscan(x_ray & board.side[turn]) };
				pin.moves[sq] = x_ray;
				pin.idx[pin.cnt++] = sq;
			}

			all_att &= all_att - 1;
		}
	}

	// clearing the pin-table

	void unpin()
	{
		if (pin.cnt != 0)
		{
			assert(pin.cnt <= 8);
			for (int i{ 0 }; i < pin.cnt; ++i)
				pin.moves[pin.idx[i]] = ~0ULL;
		}
	}

}

void eval::init()
{
	// prefilling table of pinned moves

	std::fill(pin.moves, pin.moves + 64, ~0ULL);

	// computing piece square table

	for (int p{ PAWNS }; p <= KINGS; ++p)
	{
		for (int s{ MG }; s <= EG; ++s)
		{
			for (int i{ 0 }; i < 64; ++i)
			{
				psqt[WHITE][p][s][63 - i] += value[s][p];
				psqt[BLACK][p][s][i] = psqt[WHITE][p][s][63 - i];
			}
		}
	}

	// mirroring the passed pawn table

	for (int i{ 0 }; i < 8; ++i)
	{
		passed_pawn[BLACK][i] = passed_pawn[WHITE][7 - i];
	}

	// filling the front span table

	for (int i{ 8 }; i < 56; ++i)
	{
		uint64 files{ file[i & 7] };
		if (i % 8)
			files |= file[(i - 1) & 7];
		if ((i - 7) % 8)
			files |= file[(i + 1) & 7];

		front_span[WHITE][i] = files & ~((1ULL << (i + 2)) - 1);
		front_span[BLACK][i] = files &  ((1ULL << (i - 1)) - 1);
	}
}

int eval::static_eval(pos &board)
{
	int sum[2][2]{ };

	// evaluating

	pieces(board, sum);
	pawns(board, sum);

	// interpolating

	assert(board.phase >= 0);
	int weight{ board.phase <= max_weight ? board.phase : max_weight };
	int mg_score{ sum[MG][WHITE] - sum[MG][BLACK] };
	int eg_score{ sum[EG][WHITE] - sum[EG][BLACK] };

	// 50 move rule fading

	int fading{ board.half_move_cnt <= 60 ? 40 : 100 - board.half_move_cnt };

	return negate[board.turn] * ((mg_score * weight + eg_score * (max_weight - weight)) / max_weight) * fading / 40;
}

void eval::pieces(pos &board, int sum[][2])
{
	for (int col{ WHITE }; col <= BLACK; ++col)
	{
		int not_col{ col ^ 1 };		
		int att_cnt{ 0 }, att_sum{ 0 };

		pin_down(board, col);

		uint64 king_zone{ movegen::king_table[board.king_sq[not_col]] };

		// bishops

		uint64 pieces{ board.side[col] & board.pieces[BISHOPS] };
		while (pieces)
		{
			auto sq{ bb::bitscan(pieces) };

			sum[MG][col] += psqt[not_col][BISHOPS][MG][sq];
			sum[EG][col] += psqt[not_col][BISHOPS][EG][sq];

			uint64 targets
			{
				attack::by_slider(BISHOP, sq, board.side[BOTH] & ~(board.pieces[QUEENS] & board.side[col]))
				& ~(board.side[col] & board.pieces[PAWNS]) & pin.moves[sq]
			};

			// attacking enemy king

			uint64 streak_king{ targets & ~board.side[BOTH] & king_zone };
			if (streak_king)
			{
				att_cnt += 1;
				att_sum += king_threat[BISHOPS] * bb::popcnt(streak_king);
			}

			// bishop pair

			if (pieces & (pieces - 1))
			{
				sum[MG][col] += bishop_pair[MG];
				sum[EG][col] += bishop_pair[EG];
			}

			// mobility

			int cnt{ bb::popcnt(targets) };
			sum[MG][col] += bishop_mob[MG][cnt];
			sum[EG][col] += bishop_mob[EG][cnt];

			pieces &= pieces - 1;
		}

		// rooks

		pieces = board.side[col] & board.pieces[ROOKS];
		while (pieces)
		{
			auto sq{ bb::bitscan(pieces) };

			sum[MG][col] += psqt[not_col][ROOKS][MG][sq];
			sum[EG][col] += psqt[not_col][ROOKS][EG][sq];

			uint64 targets
			{
				attack::by_slider(ROOK, sq, board.side[BOTH] & ~((board.pieces[QUEENS] | board.pieces[ROOKS]) & board.side[col]))
				& ~(board.side[col] & board.pieces[PAWNS]) & pin.moves[sq]
			};

			// attacking enemy king

			uint64 streak_king{ targets & ~board.side[BOTH] & king_zone };
			if (streak_king)
			{
				att_cnt += 1;
				att_sum += king_threat[ROOKS] * bb::popcnt(streak_king);
			}

			// being on open or semi-open file

			if (!(file[sq & 7] & board.pieces[PAWNS] & board.side[col]))
			{
				sum[MG][col] += rook_open_file;

				if (!(file[sq & 7] & board.pieces[PAWNS]))
				{
					sum[MG][col] += rook_open_file;
				}
			}

			// being on 7th rank

			if (sq >> 3 == seventh_rank[col])
			{
				if (rank[seventh_rank[col]] & board.pieces[PAWNS] & board.side[not_col]
					  || rank[R8 * not_col] & board.pieces[KINGS] & board.side[not_col])
				{
					sum[MG][col] += rook_on_7th[MG];
					sum[EG][col] += rook_on_7th[EG];
				}
			}

			// mobility

			int cnt{ bb::popcnt(targets) };
			sum[MG][col] += rook_mob[MG][cnt];
			sum[EG][col] += rook_mob[EG][cnt];

			pieces &= pieces - 1;
		}

		// queens

		pieces = board.side[col] & board.pieces[QUEENS];
		while (pieces)
		{
			auto sq{ bb::bitscan(pieces) };

			sum[MG][col] += psqt[not_col][QUEENS][MG][sq];
			sum[EG][col] += psqt[not_col][QUEENS][EG][sq];

			uint64 targets
			{
				(attack::by_slider(ROOK, sq, board.side[BOTH]) | attack::by_slider(BISHOP, sq, board.side[BOTH]))
				& ~(board.side[col] & board.pieces[PAWNS]) & pin.moves[sq]
			};

			// attacking enemy king

			uint64 streak_king{ targets & ~board.side[BOTH] & king_zone };
			if (streak_king)
			{
				att_cnt += 1;
				att_sum += king_threat[QUEENS] * bb::popcnt(streak_king);
			}

			// mobility

			int cnt{ bb::popcnt(targets) };
			sum[MG][col] += queen_mob[MG][cnt];
			sum[EG][col] += queen_mob[EG][cnt];

			pieces &= pieces - 1;
		}

		// knights

		uint64 pawn_att{ attack::by_pawns(board, not_col) & ~board.side[BOTH] };
		pieces = board.side[col] & board.pieces[KNIGHTS];
		while (pieces)
		{
			auto sq{ bb::bitscan(pieces) };

			sum[MG][col] += psqt[not_col][KNIGHTS][MG][sq];
			sum[EG][col] += psqt[not_col][KNIGHTS][EG][sq];

			uint64 targets
			{
				movegen::knight_table[sq]
				& ~(board.side[col] & board.pieces[PAWNS]) & ~pawn_att & pin.moves[sq]
			};

			// attacking enemy king

			uint64 streak_king{ targets & ~board.side[BOTH] & king_zone };
			if (streak_king)
			{
				att_cnt += 1;
				att_sum += king_threat[KNIGHTS] * bb::popcnt(streak_king);
			}

			// connected knights

			if (targets & board.pieces[KNIGHTS] & board.side[col])
			{
				sum[MG][col] += knights_connected;
				sum[EG][col] += knights_connected;
			}

			// mobility

			int cnt{ bb::popcnt(targets) };
			sum[MG][col] += knight_mob[MG][cnt];
			sum[EG][col] += knight_mob[EG][cnt];

			pieces &= pieces - 1;
		}

		// king

		sum[MG][col] += psqt[not_col][KINGS][MG][board.king_sq[col]];
		sum[EG][col] += psqt[not_col][KINGS][EG][board.king_sq[col]];

		// king safety

		int score{ (king_safety_w[att_cnt & 7] * att_sum) / 100 };
		sum[MG][col] += score;
		sum[EG][col] += score;

		unpin();
	}
}

void eval::pawns(pos &board, int sum[][2])
{
	for (int col{ WHITE }; col <= BLACK; ++col)
	{
		int not_col{ col ^ 1 };
		uint64 pawns{ board.pieces[PAWNS] & board.side[col] };
		while (pawns)
		{
			// PSQT

			auto sq{ bb::bitscan(pawns) };
			auto idx{ sq & 7 };

			sum[MG][col] += psqt[not_col][PAWNS][MG][sq];
			sum[EG][col] += psqt[not_col][PAWNS][EG][sq];

			// passing pawn

			if (!(front_span[col][sq] & board.pieces[PAWNS] & board.side[not_col]))
			{
				if (!(file[idx] & front_span[col][sq] & board.pieces[PAWNS]))
				{
					int mg_bonus{ passed_pawn[col][sq >> 3] };
					int eg_bonus{ mg_bonus };

					// blocked path

					uint64 blocked_path{ file[idx] & front_span[col][sq] & board.side[not_col] };
					if (blocked_path)
					{
						int blocker_cnt{ bb::popcnt(blocked_path) };
						mg_bonus /= blocker_cnt + 2;
						eg_bonus /= blocker_cnt + 1;
					}

					// majors behind

					uint64 pieces_behind{ file[idx] & front_span[not_col][sq] };
					uint64 majors{ (board.pieces[ROOKS] | board.pieces[QUEENS]) & board.side[col] };

					if ((pieces_behind & majors) && !(pieces_behind & (board.side[BOTH] ^ majors)))
					{
						mg_bonus += major_behind_pp[MG];
						eg_bonus += major_behind_pp[EG];
					}

					sum[MG][col] += mg_bonus;
					sum[EG][col] += eg_bonus;
				}
			}
			pawns &= pawns - 1;
		}
	}
}
