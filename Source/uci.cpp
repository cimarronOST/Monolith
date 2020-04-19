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


#include <sstream>

#include "syzygy.h"
#include "search.h"
#include "texel.h"
#include "eval.h"
#include "misc.h"
#include "time.h"
#include "bench.h"
#include "movegen.h"
#include "board.h"
#include "uci.h"

// values depending on the 'setoption' command

std::size_t uci::hash_size{ 128 };
std::size_t uci::multipv{ 1 };
int uci::thread_cnt{ 1 };
milliseconds uci::overhead{};

trans uci::hash_table{};
uci::syzygy_settings uci::syzygy { "<empty>", 5, lim::syzygy_pieces };

bool uci::use_abdada{ false };
bool uci::ponder{ false };
bool uci::chess960{ false };
bool uci::log{ false };
bool uci::use_book{ true };

// values depending on the 'go' command

bool uci::infinite{ false };
uci::search_limit uci::limit{};

// values depending on various other things

bool uci::stop{ true };
book uci::bk{};

int uci::mv_cnt{};
int uci::mv_offset{};
std::array<key64, 256> uci::game_hash{};

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

	std::string smp(bool use_abdada)
	{
		// displaying options ABDADA or Shared Hash Table

		return use_abdada ? "ABDADA" : "SHT";
	}

	std::string show_sc(score sc, bound bd)
	{
		// showing the appropriate search score

		if (sc == score::none) return "cp 0";

		std::string score{};
		if (std::abs(sc) >= score::mate - 1000)
		{
			int mate{ (sc > score::mate - 1000 ? score::mate + 1 - sc : -score::mate - 1 - sc) / 2 };
			score = "mate " + std::to_string(mate);
		}
		else
			score = "cp " + std::to_string(sc);
		if (bd == bound::upper) score += " upperbound";
		if (bd == bound::lower) score += " lowerbound";
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

		if (pv.wrong)
			std::cout << pv.mv[0].algebraic();
		else
		{
			for (depth dt{}; dt < pv.cnt; ++dt)
				std::cout << pv.mv[dt].algebraic() << " ";
		}
	}

	move convert_mv(const board& pos, const std::string& input)
	{
		// converting the coordinate move-string to the internal move-representation

		gen<mode::legal> list(pos);
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

		uci::game_hash[uci::mv_offset] = pos.key;
	}

	void set_position(board& pos, std::string fen)
	{
		// setting up a new position

		reset_game();
		pos.parse_fen(fen);
		verify(uci::mv_offset == 0);
		uci::game_hash[uci::mv_offset] = pos.key;
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
	void bench(std::istringstream& input)
	{
		// running a benchmark search

		milliseconds movetime{};
		std::string filename{};
		input >> filename;
		input >> movetime;
		reset_game();
		bench::search(filename, movetime);
	}

	void perft(std::istringstream& input, board& pos)
	{
		// running perft

		std::string mode{};
		depth dt{};
		input >> dt;
		input >> mode;

		if (mode == "pseudo")
			bench::perft<mode::pseudolegal>(pos, dt);
		else
			bench::perft<mode::legal>(pos, dt);
	}

	void eval(const board& pos)
	{
		// doing a static evaluation of the current position

		kingpawn_hash hash(kingpawn_hash::allocate_none);
		std::cout << double(eval::static_eval(pos, hash)) / 100.0 << std::endl;
	}

	void tune(std::istringstream& input)
	{
		// running the internal tuner to tune the evaluation

		reset_game();
		uci::infinite = true;
		uci::stop = false;
		int thread_cnt{};
		std::string filename{};
		input >> filename;
		input >> thread_cnt;
		texel::tune(filename, thread_cnt);
		uci::infinite = false;
		uci::stop = true;
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
	void uci()
	{
		// reacting to the 'uci' command
		// outputting all available options of the engine

		std::cout
			<< "id name Monolith " << version_number
			<< "\nid author Jonas Mayr\n"

			<< "\noption name Threads type spin default " << thread_cnt << " min 1 max " << lim::threads
			<< "\noption name SMP type combo default " << smp(use_abdada) << " var " << smp(true) << " var " << smp(false)
			<< "\noption name Ponder type check default " << boolean(ponder)
			<< "\noption name Hash type spin default " << hash_size << " min 2 max " << lim::hash
			<< "\noption name Clear Hash type button"

			<< "\noption name UCI_Chess960 type check default " << boolean(chess960)
			<< "\noption name MultiPV type spin default " << multipv << " min 1 max " << lim::multipv
			<< "\noption name Move Overhead type spin default " << overhead << " min 0 max " << lim::overhead
			<< "\noption name Log type check default " << boolean(log)

			<< "\noption name OwnBook type check default " << boolean(use_book)
			<< "\noption name Book File type string default " << bk.std_name

			<< "\noption name SyzygyPath type string default " << syzygy.path
			<< "\noption name SyzygyProbeDepth type spin default " << syzygy.dt << " min 1 max " << lim::dt
			<< "\noption name SyzygyProbeLimit type spin default " << syzygy.pieces << " min 0 max " << lim::syzygy_pieces
			<< std::endl;
	}

	void ucinewgame(board& pos, thread_pool& threads)
	{
		// responding to the 'ucinewgame' command
		// setting up a new game

		bk.hit = use_book;
		hash_table.clear();
		threads.clear_history();
		set_position(pos, startpos);
	}

	void setoption(std::istringstream& input, thread_pool& threads)
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
		else if (name == "SMP")
		{
			use_abdada = (value == "ABDADA");
		}
		else if (name == "Ponder")
		{
			ponder = boolean(value);
		}
		else if (name == "Move Overhead")
		{
			overhead = std::min(milliseconds(std::max(std::stoi(value), 0)), lim::overhead);
		}
		else if (name == "UCI_Chess960")
		{
			chess960 = boolean(value);
		}
		else if (name == "MultiPV")
		{
			multipv = std::min(std::size_t(std::max(std::stoi(value), 1)), lim::multipv);
		}
		else if (name == "OwnBook")
		{
			use_book = bk.hit = boolean(value);
		}
		else if (name == "Book File")
		{
			bk.open(value);
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
			syzygy.path = value;
			syzygy::init_tb(syzygy.path);
		}
		else if (name == "SyzygyProbeDepth")
		{
			syzygy.dt = std::min(std::max(std::stoi(value), 1), lim::dt);
		}
		else if (name == "SyzygyProbeLimit")
		{
			syzygy.pieces = std::min(std::max(std::stoi(value), 0), lim::syzygy_pieces);
		}
	}

	void position(std::istringstream& input, board& pos)
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

	void search(thread_pool& threads, board& pos, timemanage::move_time movetime)
	{
		stop = false;
		if (move bestmove{ bk.get_move(pos) }; bestmove)
		{
			// retrieving a book move

			std::cout << "info string book hit" << std::endl << "bestmove " << bestmove.algebraic() << std::endl;
			stop = true;
		}
		else
		{
			// starting the search if there is no book move

			bk.hit = false;
			threads.thread[0]->std_thread = std::thread{ search::start, std::ref(threads), movetime };
		}
	}

	void searchmoves(std::istringstream& input, const board& pos)
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

	void go(std::istringstream& input, thread_pool& threads, board& pos)
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
				input >> chrono.time[white];
				chrono.restricted = false;
			}
			else if (token == "btime")
			{
				input >> chrono.time[black];
				chrono.restricted = false;
			}
			else if (token == "winc")
			{
				input >> chrono.incr[white];
			}
			else if (token == "binc")
			{
				input >> chrono.incr[black];
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
				if (chronometer::hit_threshold > limit.nodes / thread_cnt)
					chronometer::hit_threshold = int(limit.nodes / thread_cnt);
			}
			else if (token == "depth")
			{
				input >> limit.dt;
				limit.dt = std::min(std::max(limit.dt, 1), lim::dt);
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
		search(threads, pos, chrono.get_movetime(pos.cl));
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
	bk.open(bk.std_name);

	do // communication loop
	{
		std::getline(std::cin, command);
		std::istringstream input{ command };
		input >> token;

		if (command == "uci")
		{
			uci::uci();
			std::cout << "uciok" << std::endl;
		}
		else if (command == "isready")
		{
			std::cout << "readyok" << std::endl;
		}
		else if (command == "ucinewgame")
		{
			if (!stop) continue;
			threads.stop_search();
			ucinewgame(pos, threads);
		}
		else if (token == "position")
		{
			if (!stop) continue;
			threads.stop_search();
			position(input, pos);
		}
		else if (token == "setoption")
		{
			if (!stop) continue;
			threads.stop_search();
			setoption(input, threads);
		}
		else if (token == "go")
		{
			if(!stop) continue;
			threads.stop_search();
			go(input, threads, pos);
		}
		else if (command == "stop")
		{
			threads.stop_search();
			infinite = false;
		}
		else if (command == "ponderhit")
		{
			infinite = false;
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

void uci::info_iteration(sthread& thread, int mv_cnt)
{
	// showing search informations after every iteration

	if (!thread.main())
		return;
	if (multipv > 1)
		thread.rearrange_pv(mv_cnt);

	milliseconds time{ thread.chrono.elapsed() };
	int64 nodes{ thread.get_nodes() };

	for (int i{}; i < (int)multipv && i < mv_cnt; ++i)
	{
		std::cout << "info"
			<< " depth "    << thread.pv[i].dt
			<< " seldepth " << thread.pv[i].get_seldt()
			<< show_multipv(i + 1)
			<< " score "    << show_sc(thread.pv[i].sc, bound::none)
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
	verify(bd == bound::upper || bd == bound::lower);

	if (!thread.main())
		return;

	milliseconds time{ thread.chrono.elapsed() };
	int64 nodes{ thread.get_nodes() };

	std::cout << "info"
		<< " depth "    << thread.pv[pv_n].dt
		<< " seldepth " << thread.pv[pv_n].get_seldt()
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
			<< " seldepth "       << thread.pv[pv_n].get_seldt()
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