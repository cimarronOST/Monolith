/*
  Monolith 0.2  Copyright (C) 2017 Jonas Mayr

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


#include "movegen.h"
#include "bitboard.h"
#include "evaluation.h"

namespace
{
	uint64 front_span[2][64]{ };

	const int push[]{ 8, 56 };
	const int negate[]{ 1, -1 };

	const int phase_max{ 24 };
	int phase_weight[phase_max + 1]{ };

	const int tempo_bonus[]{ eval::tempo_bonus_white, 0 };
}

void eval::init()
{
	// phase weight filling

	for (int i{ 0 }; i <= phase_max; ++i)
	{
		phase_weight[i] = i * 256 / phase_max;
	}

	// piece square table computing

	for (int p{ PAWNS }; p <= KINGS; ++p)
	{
		for (int s{ MG }; s <= EG; ++s)
		{
			for (int i{ 0 }; i < 64; ++i)
			{
				p_s_table[WHITE][p][s][63 - i] += value[s][p];
				p_s_table[BLACK][p][s][i] = p_s_table[WHITE][p][s][63 - i];
			}
		}
	}

	// passed pawn table mirroring

	for (int i{ 0 }; i < 64; ++i)
	{
		passed_pawn[BLACK][i] = passed_pawn[WHITE][63 - i];
	}

	// front span filling

	for (int i{ 8 }; i < 56; ++i)
	{
		uint64 files{ file[i & 7] };
		if (i % 8) files |= file[(i - 1) & 7];
		if ((i - 7) % 8) files |= file[(i + 1) & 7];

		front_span[WHITE][i] = files & ~((1ULL << (i + 2)) - 1);
		front_span[BLACK][i] = files &  ((1ULL << (i - 1)) - 1);
	}
}

int eval::eval_board(pos &board)
{
	int sum[2][2]{ };

	// evaluate

	pieces(board, sum);
	pawns(board, sum);

	// tempo

	sum[MG][WHITE] += tempo_bonus[board.turn];

	// interpolate

	assert(board.phase >= 0);
	int phase{ board.phase <= phase_max ? board.phase : phase_max };
	int &weight{ phase_weight[phase] };
	int mg_score{ sum[MG][WHITE] - sum[MG][BLACK] };
	int eg_score{ sum[EG][WHITE] - sum[EG][BLACK] };

	// 50 move rule

	int fading{ 40 };
	if (board.half_moves > 60)
		fading -= board.half_moves - 60;

	return negate[board.turn] * ((mg_score * weight + eg_score * (256 - weight)) >> 8) * fading / 40;
}

void eval::pieces(pos &board, int sum[][2])
{
	int old_turn{ board.turn };

	for (int col{ WHITE }; col <= BLACK; ++col)
	{
		board.turn = col;
		int not_col{ col ^ 1 };
		int att_cnt{ 0 };
		int att_sum{ 0 };

		movegen::legal init;
		init.pin_down(board);

		uint64 king_zone{ movegen::king_table[board.king_sq[col ^ 1]] };

		// bishops

		uint64 pieces{ board.side[col] & board.pieces[BISHOPS] };
		while (pieces)
		{
			bb::bitscan(pieces);
			auto sq{ bb::lsb() };

			sum[MG][col] += p_s_table[not_col][BISHOPS][MG][sq];
			sum[EG][col] += p_s_table[not_col][BISHOPS][EG][sq];

			uint64 targets
			{
				movegen::slide_att(BISHOP, sq, board.side[BOTH] & ~(board.pieces[QUEENS] & board.side[col]))
				& ~(board.side[col] & board.pieces[PAWNS]) & movegen::pinned[sq]
			};

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
			bb::bitscan(pieces);
			auto sq{ bb::lsb() };

			sum[MG][col] += p_s_table[not_col][ROOKS][MG][sq];
			sum[EG][col] += p_s_table[not_col][ROOKS][EG][sq];

			uint64 targets
			{
				movegen::slide_att(ROOK, sq, board.side[BOTH] & ~((board.pieces[QUEENS] | board.pieces[ROOKS]) & board.side[col]))
				& ~(board.side[col] & board.pieces[PAWNS]) & movegen::pinned[sq]
			};

			uint64 streak_king{ targets & ~board.side[BOTH] & king_zone };
			if (streak_king)
			{
				att_cnt += 1;
				att_sum += king_threat[ROOKS] * bb::popcnt(streak_king);
			}

			// rook on open or semi-open file

			if (!(file[sq & 7] & board.pieces[PAWNS] & board.side[col]))
			{
				sum[MG][col] += rook_open_file;

				if (!(file[sq & 7] & board.pieces[PAWNS]))
				{
					sum[MG][col] += rook_open_file;
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
			bb::bitscan(pieces);
			auto sq{ bb::lsb() };

			sum[MG][col] += p_s_table[not_col][QUEENS][MG][sq];
			sum[EG][col] += p_s_table[not_col][QUEENS][EG][sq];

			uint64 targets
			{
				(movegen::slide_att(ROOK, sq, board.side[BOTH]) | movegen::slide_att(BISHOP, sq, board.side[BOTH]))
				& ~(board.side[col] & board.pieces[PAWNS]) & movegen::pinned[sq]
			};

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

		uint64 pawn_att{ attack::by_pawns(board, col ^ 1) & ~board.side[BOTH] };
		pieces = board.side[col] & board.pieces[KNIGHTS];
		while (pieces)
		{
			bb::bitscan(pieces);
			auto sq{ bb::lsb() };

			sum[MG][col] += p_s_table[not_col][KNIGHTS][MG][sq];
			sum[EG][col] += p_s_table[not_col][KNIGHTS][EG][sq];

			uint64 targets
			{
				movegen::knight_table[sq]
				& ~(board.side[col] & board.pieces[PAWNS]) & ~pawn_att & movegen::pinned[sq]
			};

			uint64 streak_king{ targets & ~board.side[BOTH] & king_zone };
			if (streak_king)
			{
				att_cnt += 1;
				att_sum += king_threat[KNIGHTS] * bb::popcnt(streak_king);
			}

			// connected knights

			if (targets & board.pieces[KNIGHTS] & board.side[col])
			{
				sum[MG][col] += knights_connected[MG];
				sum[EG][col] += knights_connected[EG];
			}

			// mobility

			int cnt{ bb::popcnt(targets) };
			sum[MG][col] += knight_mob[MG][cnt];
			sum[EG][col] += knight_mob[EG][cnt];

			pieces &= pieces - 1;
		}

		// king

		sum[MG][col] += p_s_table[not_col][KINGS][MG][board.king_sq[col]];
		sum[EG][col] += p_s_table[not_col][KINGS][EG][board.king_sq[col]];

		// king safety

		int score{ (king_safety_w[att_cnt & 7] * att_sum) / 100 };
		sum[MG][col] += score;
		sum[EG][col] += score;
	}

	board.turn = old_turn;
}
void eval::pawns(pos &board, int sum[][2])
{
	for (int col{ WHITE }; col <= BLACK; ++col)
	{
		int not_col{ col ^ 1 };
		uint64 pawns{ board.pieces[PAWNS] & board.side[col] };
		while (pawns)
		{
			bb::bitscan(pawns);
			auto sq{ bb::lsb() };

			sum[MG][col] += p_s_table[not_col][PAWNS][MG][sq];
			sum[EG][col] += p_s_table[not_col][PAWNS][EG][sq];

			// is the pawn passed?

			if (!(front_span[col][sq] & board.pieces[PAWNS] & board.side[not_col]))
			{
				// is no friendly pawn ahead?

				auto idx{ sq & 7 };
				if (!(file[idx] & front_span[col][sq] & board.pieces[PAWNS]))
				{
					int mg_bonus{ passed_pawn[not_col][sq] };
					int eg_bonus{ mg_bonus };

					// is the path blocked?

					uint64 blocked_path{ file[idx] & front_span[col][sq] & board.side[not_col] };
					if (blocked_path)
					{
						int blocker_cnt{ bb::popcnt(blocked_path) };
						mg_bonus /= blocker_cnt + 2;
						eg_bonus /= blocker_cnt + 1;
					}

					// are majors behind?

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
