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


#include <sstream>
#include <string>
#include <chrono>
#include <algorithm>
#include <iostream>
#include <vector>
#include <functional>
#include <thread>

#include "main.h"
#include "types.h"
#include "trans.h"
#include "thread.h"
#include "syzygy.h"
#include "search.h"
#include "tune.h"
#include "eval.h"
#include "misc.h"
#include "time.h"
#include "bench.h"
#include "movegen.h"
#include "board.h"
#include "uci.h"

namespace
{
	bool boolean(const std::string& value)
	{
		// converting string to bool

		verify(value == "true" || value == "false");
		return value == "true";
	}

	std::string boolean(bool value)
	{
		// converting bool to string

		return value ? "true" : "false";
	}

	std::string show_sc(score sc, bound bd)
	{
		// showing the appropriate search score

		if (sc == score::NONE) return "cp 0";

		std::string score{};
		if (std::abs(sc) >= MATE - 1000)
		{
			int mate{ (sc > MATE - 1000 ? MATE + 1 - sc : -MATE - 1 - sc) / 2 };
			score = "mate " + std::to_string(mate);
		}
		else
			score = "cp " + std::to_string(sc);
		if (bd == bound::UPPER) score += " upperbound";
		if (bd == bound::LOWER) score += " lowerbound";
		return score;
	}

	std::string show_multipv(int pv)
	{
		// showing the multipv extension only when demanded

		return uci::multipv > 1 ? " multipv " + std::to_string(pv) : "";
	}

	std::string show_hashfull(milliseconds time)
	{
		// showing the hash table occupation after at least 1 second of search

		return time >= milliseconds(1000) ? " hashfull " + std::to_string(trans::hashfull()) : "";
	}

	void show_variation(const move_var& pv)
	{
		// showing the principal variation

		if (pv.tb_root)
			std::cout << pv.mv[0].algebraic();
		else
		{
			for (depth dt{}, dt_max{ std::min(pv.cnt, lim::dt) }; dt < dt_max; ++dt)
				std::cout << pv.mv[dt].algebraic() << " ";
		}
	}

	move convert_mv(const board& pos, const std::string& input)
	{
		// converting the coordinate move-string to the internal move-representation

		gen<mode::LEGAL> list(pos);
		list.gen_all();

		for (int i{}; i < list.cnt.mv; ++i)
		{
			if (input == list.mv[i].algebraic())
				return list.mv[i];
		}
		return move{};
	}

	void reset_game()
	{
		// resetting all game-specific parameters used to detect repetitions

		uci::mv_cnt = 0;
		uci::mv_offset = 0;
		for (auto& hash : uci::game_hash) hash = 0ULL;
	}

	void save_move(const board& pos)
	{
		// keeping track of the half move count & transpositions to detect repetitions

		uci::mv_cnt += 1;
		uci::mv_offset = pos.half_cnt ? uci::mv_offset + 1 : 0;
		verify(uci::mv_offset <= pos.half_cnt);

		uci::game_hash[uci::mv_offset] = pos.key.pos;
	}

	void set_position(board& pos, std::string fen)
	{
		// setting up a new position

		reset_game();
		pos.parse_fen(fen);
		verify(uci::mv_offset == 0);
		uci::game_hash[uci::mv_offset] = pos.key.pos;
	}

	void new_move(board& pos, move mv)
	{
		// doing & saving a new move

		pos.new_move(mv);
		save_move(pos);
	}
}

namespace debug
{
	static void bench(std::istringstream& input)
	{
		// running a benchmark search

		milliseconds movetime{};
		std::string filename{};
		input >> filename;
		input >> movetime;
		reset_game();
		uci::hash_table.clear();
		bench::search(filename, movetime);
	}

	static void perft(std::istringstream& input, board& pos)
	{
		// running perft

		std::string mode{};
		depth dt{};
		input >> dt;
		input >> mode;

		if (mode == "pseudo")
			bench::perft<mode::PSEUDOLEGAL>(pos, dt);
		else
			bench::perft<mode::LEGAL>(pos, dt);
	}

	static void eval(const board& pos)
	{
		// doing a static evaluation of the current position

		kingpawn_hash hash(kingpawn_hash::ALLOCATE_NONE);
		std::cout << double(eval::static_eval(pos, hash)) / 100.0 << std::endl;
	}

	static void tune(std::istringstream& input)
	{
		// running the internal tuner to tune the evaluation function

		reset_game();
		uci::hash_table.clear();
		uci::infinite = true;
		uci::stop = false;

		std::vector<std::string> tuning_files{};
		std::string file{};
		while (input >> file)
			tuning_files.emplace_back(file);
		tune::evaluation(tuning_files, uci::thread_cnt);

		uci::infinite = false;
		uci::stop = true;

		// running the internal benchmark search with the now tuned evaluation

		bench(input);
	}

	[[maybe_unused]] static void show_search_params()
	{
		// printing all search parameters to tune with SPSA
		
#if !defined(NDEBUG)
		for (auto& par : search::s_param)
			std::cout << "\noption name " << par.name << " type spin default " << par.value
			<< " min " << par.min << " max " << par.max;
		std::cout << std::endl;
#endif
	}
}

void uci::search_limit::set_infinite()
{
	// setting all search limits to infinite at the beginning of the 'go' command

	uci::limit.searchmoves.clear();
	uci::limit.movetime = lim::movetime;
	uci::limit.nodes = lim::nodes;
	uci::limit.dt = lim::dt;
	uci::limit.mate = 0;
}

namespace uci
{
	static void uci()
	{
		// reacting to the 'uci' command
		// outputting all available options of the engine

		std::cout
			<< "id name Monolith " << version_number
			<< "\nid author Jonas Mayr\n"

			<< "\noption name Threads type spin default " << thread_cnt << " min 1 max " << lim::threads
			<< "\noption name Ponder type check default " << boolean(ponder)
			<< "\noption name Hash type spin default " << hash_size << " min 2 max " << lim::hash
			<< "\noption name Clear Hash type button"

			<< "\noption name UCI_Chess960 type check default " << boolean(chess960)
			<< "\noption name MultiPV type spin default " << multipv << " min 1 max " << lim::multipv
			<< "\noption name Move Overhead type spin default " << overhead << " min 0 max " << lim::overhead
			<< "\noption name Log type check default " << boolean(log)

			<< "\noption name SyzygyPath type string default " << syzygy_path
			<< "\noption name SyzygyProbeDepth type spin default " << syzygy_dt << " min 1 max " << lim::dt
			<< std::endl;
	}

	static void ucinewgame(board& pos, thread_pool& threads)
	{
		// responding to the 'ucinewgame' command
		// setting up a new game

		hash_table.clear();
		threads.clear_history();
		set_position(pos, startpos);
	}

	static void setoption(std::istringstream& input, thread_pool& threads)
	{
		// parsing the 'setoption' command

		std::string token{}, name{}, value{};
		input >> name;
		input >> name;

		while (input >> token && token != "value")
			name += " " + token;
		input >> value;
		while (input >> token)
			value += " " + token;

		// setting the new option

		if (name == "Hash")
		{
			hash_size = hash_table.create(std::max(std::stoi(value), 2));
		}
		else if (name == "Clear Hash")
		{
			hash_table.clear();
		}
		else if (name == "Threads")
		{
			thread_cnt = std::max(std::stoi(value), 1);
			threads.resize(thread_cnt);
			threads.start_all();
		}
		else if (name == "Ponder")
		{
			ponder = boolean(value);
		}
		else if (name == "Move Overhead")
		{
			overhead = std::clamp(milliseconds(std::stoi(value)), milliseconds(0), lim::overhead);
		}
		else if (name == "UCI_Chess960")
		{
			chess960 = boolean(value);
		}
		else if (name == "MultiPV")
		{
			multipv = std::clamp(std::size_t(std::stoi(value)), std::size_t(1), lim::multipv);
		}
		else if (name == "Log")
		{
			static synclog sync;
			log = boolean(value);
			if (log) sync.start();
			else     sync.stop();
		}
		else if (name == "SyzygyPath")
		{
			syzygy_path = value;
			syzygy::init_tb(syzygy_path);
		}
		else if (name == "SyzygyProbeDepth")
		{
			syzygy_dt = std::clamp(std::stoi(value), 1, lim::dt);
		}
		else
		{
			// setting the search parameters during SPSA tuning

#if !defined(NDEBUG)
			for (auto& par : search::s_param)
			{
				if (name == par.name)
				{
					par.value = std::stoi(value);
					search::init_params();
				}
			}
#endif
		}
	}

	static void position(std::istringstream& input, board& pos)
	{
		// reacting to the 'position' command

		std::string token{}, fen{};
		input >> token;

		if (token == "startpos")
		{
			fen = startpos;
			input >> token;
		}
		else if (token == "fen")
		{
			while (input >> token && token != "moves")
				fen += token + " ";
		}

		set_position(pos, fen);
		if (token != "moves")
			return;

		// executing the move sequence

		while (input >> token)
			new_move(pos, convert_mv(pos, token));
	}

	static void searchmoves(std::istringstream& input, const board& pos)
	{
		// parsing the 'go searchmoves' command

		std::string token{};
		while (input >> token)
		{
			move mv{ convert_mv(pos, token) };
			if (!mv)
			{
				std::string remains{ token };
				while (input >> token)
					remains += " " + token;

				input.str(remains);
				input.clear();
				break;
			}
			limit.searchmoves.push_back(mv);
		}
	}

	static void go(std::istringstream& input, thread_pool& threads, board& pos)
	{
		// applying specifications before starting the search after the 'go' command

		std::string token{};
		timemanage chrono{};
		chronometer::reset_hit_threshold();
		limit.set_infinite();

		while (input >> token)
		{
			if (token == "movestogo")
			{
				input >> chrono.movestogo;
			}
			else if (token == "wtime")
			{
				input >> chrono.time[WHITE];
				chrono.restricted = false;
			}
			else if (token == "btime")
			{
				input >> chrono.time[BLACK];
				chrono.restricted = false;
			}
			else if (token == "winc")
			{
				input >> chrono.incr[WHITE];
			}
			else if (token == "binc")
			{
				input >> chrono.incr[BLACK];
			}
			else if (token == "ponder")
			{
				infinite = true;
				chrono.restricted = false;
			}
			// special restriction cases

			else if (token == "searchmoves")
			{
				searchmoves(input, pos);
			}
			else if (token == "nodes")
			{
				input >> limit.nodes;
				chronometer::hit_threshold = thread_cnt;
			}
			else if (token == "depth")
			{
				input >> limit.dt;
				limit.dt = std::clamp(limit.dt, 1, lim::dt);
			}
			else if (token == "mate")
			{
				input >> limit.mate;
			}
			else if (token == "movetime")
			{
				input >> limit.movetime;
				chrono.movetime.target = limit.movetime;
			}
			else if (token == "infinite")
			{
				infinite = true;
			}
		}

		// starting the search for the best move

		stop = false;
		threads.thread[0]->std_thread = std::jthread{ search::start, std::ref(threads), chrono.get_movetime(pos.cl) };
	}
}

void uci::loop()
{
	// communication loop using the UCI protocol
	// initializing objects first

	std::string command{}, token{};
	board pos{};
	thread_pool threads(thread_cnt, pos);
	threads.start_all();
	ucinewgame(pos, threads);

	do // communication loop
	{
		std::getline(std::cin, command);
		std::istringstream input{ command };
		input >> token;

		if (command == "uci")
		{
			uci::uci();
			debug::show_search_params();
			std::cout << "uciok" << std::endl;
		}
		else if (command == "isready")
		{
			std::cout << "readyok" << std::endl;
		}
		else if (command == "ucinewgame")
		{
			if (!threads.join_main()) continue;
			ucinewgame(pos, threads);
		}
		else if (token == "position")
		{
			if (!threads.join_main()) continue;
			position(input, pos);
		}
		else if (token == "setoption")
		{
			if (!threads.join_main()) continue;
			setoption(input, threads);
		}
		else if (token == "go")
		{
			if (!threads.join_main()) continue;
			go(input, threads, pos);
		}
		else if (command == "stop")
		{
			stop = true;
			infinite = false;
			cv.notify_one();
			threads.join_main();
		}
		else if (command == "ponderhit")
		{
			infinite = false;
			cv.notify_one();
		}

		// unofficial commands for debugging

		else if (command == "bench")
		{
			// running a benchmark consisting of various search positions
			// 'bench' or 'bench [positions.epd] [time in ms]'

			if (!stop) continue;
			debug::bench(input);
		}
		else if (token == "perft")
		{
			// running perft on a position
			// 'perft [depth] legal/pseudo'

			if (!stop) continue;
			debug::perft(input, pos);
		}
		else if (command == "eval")
		{
			// showing the static evaluation of the current position

			if (!stop) continue;
			debug::eval(pos);
		}
		else if (command == "board")
		{
			// outputting the current's board piece placement

			if (!stop) continue;
			pos.display();
		}
		else if (token == "tune")
		{
			// tuning evaluation parameters if the TUNE compiler-switch is on
			// 'tune [positions.epd] [number of threads]'

			debug::tune(input);
		}
	} while (command != "quit");
}

void uci::info_iteration(sthread& thread)
{
	// showing search information after every iteration

	if (multipv > 1)
		thread.rearrange_pv();

	milliseconds time{ thread.chrono.elapsed() };
	int64 nodes{ thread.get_nodes() };

	for (int i{}; i < (int)multipv && i < thread.cnt_root_mv; ++i)
	{
		std::cout << "info"
			<< " depth "    << thread.pv[i].dt
			<< " seldepth " << std::max(thread.seldt, thread.pv[i].dt)
			<< show_multipv(i + 1)
			<< " score "    << show_sc(thread.pv[i].sc, bound::NONE)
			<< " time "     << time
			<< " nodes "    << nodes
			<< " nps "      << nodes * 1000 / std::max((int64)time.count(), 1LL)
			<< show_hashfull(time)
			<< " tbhits "   << thread.get_tbhits()
			<< " pv ";         show_variation(thread.pv[i]);
		std::cout << std::endl;
	}
}

void uci::info_bound(sthread& thread, int pv_n, score sc, bound bd)
{
	// showing search-bound information at every fail-high & fail-low inside the aspiration window

	verify(type::sc(sc));
	verify(bd == bound::UPPER || bd == bound::LOWER);

	if (!thread.main())
		return;

	milliseconds time{ thread.chrono.elapsed() };
	int64 nodes{ thread.get_nodes() };

	std::cout << "info"
		<< " depth "    << thread.pv[pv_n].dt
		<< " seldepth " << std::max(thread.seldt, thread.pv[pv_n].dt)
		<< show_multipv(pv_n + 1)
		<< " score "    << show_sc(sc, bd)
		<< " time "     << time
		<< " nodes "    << nodes
		<< " nps "      << nodes * 1000 / std::max((int64)time.count(), 1LL)
		<< show_hashfull(time)
		<< std::endl;
}

void uci::info_currmove(sthread& thread, int pv_n, move mv, int mv_n)
{
	// showing the currently searched move

	if (!thread.main())
		return;
	if (thread.chrono.elapsed() > milliseconds(5000))
	{
		std::cout << "info"
			<< " depth "          << thread.pv[pv_n].dt
			<< " seldepth "       << std::max(thread.seldt, thread.pv[pv_n].dt)
			<< show_multipv(pv_n + 1)
			<< " currmove "       << mv.algebraic()
			<< " currmovenumber " << mv_n
			<< std::endl;
	}
}

void uci::info_bestmove(std::tuple<move, move> mv)
{
	// showing best-move and ponder-move at the end of a search

	std::cout
		<< "bestmove " << std::get<0>(mv).algebraic()
		<< (std::get<1>(mv) ? " ponder " + std::get<1>(mv).algebraic() : "") << std::endl;
}