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
#include <vector>

#include "time.h"
#include "move.h"
#include "thread.h"
#include "board.h"
#include "types.h"

// tuning of the search parameters is achieved by simultaneous perturbation stochastic approximation (SPSA)

#if !defined(NDEBUG)
#define SPSA(name, value, min, max, step) inline create_parameter name(#name, value, min, max, step);
#else
#define SPSA(name, value, min, max, step) constexpr int name = value;
#endif

// searching for the best move

namespace search
{
    struct node
    {
        board* pos;
        struct p_variation{ std::array<move, lim::dt> mv; int cnt; }* p_var;
        bool check;
        bool cut;
        bool pv;
    };
    inline int64 bench{};

    // late move pruning & reduction table, indexed by depth

    inline std::array<std::array<depth, 7>, 2> late_move_cnt;
    inline std::array<std::array<depth, lim::moves>, lim::dt + 1> late_move_red;

    void init_params();

    // start functions for the alphabeta-search and the quiescence-search

    score qsearch(sthread& thread, sstack* stack, node nd, depth dt, score alpha, score beta);
    void start(thread_pool& threads, timemanage::move_time mv_time);

    // search parameters have to be visible for the tuner

    struct parameter
    {
        std::string name;
        int& value;
        int min;
        int max;
        int step;
    };

    inline std::vector<parameter> s_param;

    struct create_parameter
    {
        int v;
        create_parameter(std::string name, int value, int min, int max, int step)
            : v(value) { s_param.push_back({ name, v, min, max, step }); }
        constexpr operator int() { return v; }
    };

    // tunable search parameters

    SPSA(DELTA_MARGIN, 100, 0, 150, 10);
    SPSA(IIR_DT, 5, 4, 9, 1);
    SPSA(SNMP_DT, 7, 3, 8, 1);
    SPSA(SNMP_MARGIN1, 26, 10, 80, 5);
    SPSA(SNMP_MARGIN2, 6, 3, 12, 1);
    SPSA(RAZOR_DT, 4, 1, 6, 1);
    SPSA(RAZOR_MARGIN1, 264, 150, 300, 10);
    SPSA(RAZOR_MARGIN2, 93, 30, 150, 10);
    SPSA(NMP_DT, 3, 1, 5, 1);
    SPSA(NMP_RED1, 3, 2, 4, 1);
    SPSA(NMP_RED2, 2, 2, 6, 1);
    SPSA(NMP_RED3, 129, 80, 250, 10);
    SPSA(IID_DT, 6, 1, 8, 1);
    SPSA(IID_RED, 5, 1, 5, 1);
    SPSA(FUT_DT, 7, 2, 8, 1);
    SPSA(FUT_MARGIN1, 73, 20, 100, 10);
    SPSA(FUT_MARGIN2, 50, 0, 100, 10);
    SPSA(LMP_DT, 6, 3, 6, 1);
    SPSA(LMP01,  6,  2,  7, 1);
    SPSA(LMP02,  6,  2,  8, 1);
    SPSA(LMP03,  5,  4, 10, 1);
    SPSA(LMP04, 15,  7, 15, 1);
    SPSA(LMP05, 21, 13, 23, 1);
    SPSA(LMP06, 23, 15, 30, 1);
    SPSA(LMP11,  7,  3,  9, 1);
    SPSA(LMP12,  9,  7, 13, 1);
    SPSA(LMP13, 13, 10, 17, 1);
    SPSA(LMP14, 18, 14, 24, 1);
    SPSA(LMP15, 27, 24, 36, 1);
    SPSA(LMP16, 49, 40, 55, 1);
    SPSA(CONT_HIST_DT, 3, 1, 5, 1);
    SPSA(CONT_HIST_MARGIN1, -758, -1000, -100, 50);
    SPSA(CONT_HIST_MARGIN2, -2276, -4000, -1000, 50);
    SPSA(SEE_QUIET_DT, 12, 6, 14, 1);
    SPSA(SEE_QUIET_MARGIN, -12, -100, 0, 5);
    SPSA(HIST_DT, 3, 1, 4, 1);
    SPSA(HIST_MARGIN, -245, -500, 0, 10);
    SPSA(SEE_TAC_DT, 6, 1, 6, 1);
    SPSA(SEE_TAC_MARGIN, -67, -200, 0, 25);
    SPSA(SE_DT, 8, 7, 13, 1);
    SPSA(SE_RED, 5, 2, 6, 1);
    SPSA(HIST_EXT_DT, 8, 4, 10, 1);
    SPSA(HIST_EXT_MARGIN, 10153, 5000, 15000, 100);
    SPSA(LMR_START, 90, 0, 200, 10);
    SPSA(LMR_BASE, 45, 20, 60, 1);
    SPSA(LMR_DT, 3, 1, 5, 1);
    SPSA(LMR_CNT, 3, 2, 6, 1);
    SPSA(LMR_MAX, 6, 3, 9, 1);
    SPSA(HIST_RED, -5907, -10000, -2000, 100);
    SPSA(ALPHA_MARGIN, 324, 150, 500, 10);
    SPSA(FAIL_HIGH_CNT, 3, 1, 6, 1);
    SPSA(DEEPER_MARGIN, 45, 10, 100, 10);
    SPSA(SHALLOWER_MARGIN, 1, 0, 30, 10);
    SPSA(ASP_WINDOW, 33, 15, 45, 2);
    SPSA(ASP_MULT, 2, 2, 6, 1);
    SPSA(ASP_MULT_MAX, 6, 2, 16, 2);
    SPSA(EXT_TIME_DT, 2, 2, 8, 1);
    SPSA(EXT_TIME_MARGIN, -22, -50, -10, 5);
    SPSA(TARGET_MOVES, 20, 10, 30, 1);
    SPSA(TOLERABLE_DIV, 4, 2, 8, 1);
    SPSA(EXTEND_TIME, 10, 6, 18, 1);
    SPSA(SEE_V_PAWN, 85, 60, 120, 2);
    SPSA(SEE_V_KNIGHT, 324, 300, 400, 5);
    SPSA(SEE_V_BISHOP, 343, 300, 400, 5);
    SPSA(SEE_V_ROOK, 579, 450, 650, 5);
    SPSA(SEE_V_QUEEN, 1139, 900, 1300, 10);
    SPSA(HIST_BASE, 4, 2, 8, 1);
    SPSA(HIST_GRAVITY, 8, 4, 10, 1);
    SPSA(BASE_BONUS, 12, 0, 50, 5);
    SPSA(BASE_MALUS, -7, -50, 0, 5);
    SPSA(CORR_BASE, 1, 0, 5, 1);
    SPSA(CORR_GRAVITY, 9, 6, 14, 1);
    SPSA(CORR_BONUS, 2, 1, 6, 1);
    SPSA(CORR_PAWN, 7, 2, 16, 1);
    SPSA(CORR_MINOR, 4, 2, 16, 1);
    SPSA(CORR_MAJOR, 4, 2, 16, 1);
    SPSA(CORR_NONPAWN_W, 6, 2, 16, 1);
    SPSA(CORR_NONPAWN_B, 8, 2, 16, 1);
    SPSA(CORR_CONT2, 5, 2, 16, 1);
    SPSA(CORR_CONT3, 4, 2, 16, 1);
    SPSA(CORR_CONT4, 4, 2, 16, 1);
    SPSA(CORR_CONT5, 3, 2, 16, 1);
}