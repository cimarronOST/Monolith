/*
  Monolith 1.0  Copyright (C) 2017-2018 Jonas Mayr

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


#include "utilities.h"
#include "thread.h"
#include "texel.h"
#include "eval.h"
#include "search.h"
#include "polyglot.h"
#include "trans.h"
#include "chronos.h"
#include "move.h"
#include "movegen.h"
#include "bench.h"
#include "stream.h"
#include "syzygy.h"
#include "uci.h"

// values depending on the 'setoption' command

int uci::hash_size{ 128 };
int uci::thread_count{ 1 };
int uci::multipv{ 1 };
int uci::overhead{};
int uci::contempt[]{ SCORE_DRAW, SCORE_DRAW };

struct uci::syzygy_settings uci::syzygy{ "<empty>", 1, lim::syzygy_pieces, true };

bool uci::ponder  { false };
bool uci::chess960{ false };
bool uci::use_book{ true };

// values depending on the 'go' command

bool uci::infinite{ false };
struct uci::search_limit uci::limit{};

// values depending on various other things

int uci::move_count{};
int uci::move_offset{};

uint64 uci::quiet_hash[256]{};

bool uci::stop{ true };
bool uci::bookmove{ true };

namespace hash
{
	// main transposition hash-table

	trans table(uci::hash_size);
}

namespace limits
{
	void infinite()
	{
		// resetting all search limits to their limit at every 'go' command

		uci::limit.moves.clear();
		uci::limit.nodes = lim::nodes;
		uci::limit.depth = lim::depth;
		uci::limit.mate  = 0;
	}
}

namespace value
{
	bool boolean(std::string &value)
	{
		assert(value == "true" || value == "false");
		return value == "true";
	}

	std::string boolean(bool value)
	{
		return value ? "true" : "false";
	}
}

namespace move
{
	uint32 encode(board &pos, std::string input)
	{
		// encoding the coordinate move-string into the internal move-representation

		gen list(pos, LEGAL);
		list.gen_all();

		for (int i{}; i < list.moves; ++i)
		{
			if (input == move::algebraic(list.move[i]))
				return list.move[i];
		}
		return MOVE_NONE;
	}

	void save(const board &pos)
	{
		// keeping track of the half move count & transpositions to detect repetitions

		uci::move_offset = { pos.half_count ? uci::move_offset + 1 : 0 };
		uci::move_count += 1;
		assert(uci::move_offset <= pos.half_count);

		uci::quiet_hash[uci::move_offset] = pos.key;
	}
}

namespace display
{
	void position(board &pos)
	{
		// displaying the piece placement of the position

		std::string row{ "+---+---+---+---+---+---+---+---+" };
		sync::cout << row << "\n";
		for (int sq{ A8 }; sq >= H1; --sq)
		{
			auto piece{ "PNBRQK  "[pos.piece[sq]] };
			sync::cout << "| " << ((1ULL << sq) & pos.side[BLACK] ? static_cast<char>(tolower(piece)) : piece) << " ";
			if (sq % 8 == 0)
				sync::cout << "|\n" << row << std::endl;
		}
	}
}

namespace game
{
	void new_move(board &pos, uint32 move)
	{
		// doing & saving a new move

		pos.new_move(move);
		move::save(pos);
	}

	void reset()
	{
		// resetting all game-specific parameters used detect repetitions

		uci::move_count  = 0;
		uci::move_offset = 0;
		for (auto &hash : uci::quiet_hash) hash = 0ULL;
	}
}

namespace uci
{
	void search(thread_pool &threads, board &pos, int64 movetime)
	{
		// retrieving a book move

		stop = false;
		uint32 bestmove{ bookmove ? book::get_move(pos) : (uint32)MOVE_NONE };
		if (bestmove != MOVE_NONE)
		{
			sync::cout << "bestmove " << move::algebraic(bestmove) << "\ninfo string book hit" << std::endl;
			stop = true;
		}

		// starting the search if there is no book move

		else
		{
			bookmove = false;
			analysis::reset();
			threads.thread[MAIN]->std_thread = std::thread{ search::start, std::ref(threads), movetime };
		}
	}

	void searchmoves(std::istringstream &stream, board &pos)
	{
		// parsing the 'go searchmoves' command

		std::string token;
		while (stream >> token)
		{
			auto move{ move::encode(pos, token) };
			if (move == MOVE_NONE)
			{
				std::string remains{ token };
				while (stream >> token)
					remains += " " + token;

				stream.str(remains);
				stream.clear();
				break;
			}
			limit.moves.push_back(move);
		}
	}

	void go(std::istringstream &stream, thread_pool &threads, board &pos)
	{
		// applying specifications before starting the search after the 'go' command

		std::string token;
		chronomanager chrono;
		limits::infinite();

		while (stream >> token)
		{
			if (token == "movestogo")
			{
				stream >> chrono.moves_to_go;
			}
			else if (token == "wtime")
			{
				stream >> chrono.time[WHITE];
				chrono.infinite = false;
			}
			else if (token == "btime")
			{
				stream >> chrono.time[BLACK];
				chrono.infinite = false;
			}
			else if (token == "winc")
			{
				stream >> chrono.incr[WHITE];
			}
			else if (token == "binc")
			{
				stream >> chrono.incr[BLACK];
			}
			else if (token == "ponder")
			{
				infinite = true;
				chrono.infinite = false;
			}

			// special cases

			else if (token == "searchmoves")
			{
				searchmoves(stream, pos);
			}
			else if (token == "nodes")
			{
				stream >> limit.nodes;
			}
			else if (token == "depth")
			{
				stream >> limit.depth;
				limit.depth = value::minmax(limit.depth, 1, lim::depth);
			}
			else if (token == "mate")
			{
				stream >> limit.mate;
			}
			else if (token == "movetime")
			{
				stream >> chrono.movetime;
			}
			else if (token == "infinite")
			{
				infinite = true;
			}
		}

		search(threads, pos, chrono.get_movetime(pos.turn));
	}

	void setoption(std::istringstream &stream, thread_pool &threads, board &pos)
	{
		// parsing the 'setoption' command

		std::string token, name, value;
		stream >> name;
		stream >> name;

		while (stream >> token && token != "value")
			name += " " + token;
		stream >> value;
		while (stream >> token)
			value += " " + token;

		// setting the new option

		if (name == "Hash")
		{
			hash_size = hash::table.create(std::max(stoi(value), 1));
		}
		else if (name == "Clear Hash")
		{
			hash::table.clear();
		}
		else if (name == "Contempt")
		{
			auto new_contempt{ value::minmax(stoi(value), lim::min_contempt, lim::max_contempt) };
			contempt[pos.turn]  =  new_contempt;
			contempt[pos.xturn] = -new_contempt;
		}
		else if (name == "Threads")
		{
			thread_count = std::max(stoi(value), 1);
			threads.resize(thread_count);
			threads.start_all();
		}
		else if (name == "Ponder")
		{
			ponder = value::boolean(value);
		}
		else if (name == "Move Overhead")
		{
			overhead = value::minmax(stoi(value), 0, lim::overhead);
		}
		else if (name == "UCI_Chess960")
		{
			chess960 = value::boolean(value);
		}
		else if (name == "MultiPV")
		{
			multipv = value::minmax(stoi(value), 1, lim::multipv);
		}
		else if (name == "OwnBook")
		{
			use_book = bookmove = value::boolean(value);
		}
		else if (name == "Book File")
		{
			book::name = value;
			open_book();
		}
		else if (name == "SyzygyPath")
		{
			syzygy.path = value;
			syzygy::init_tablebases(syzygy.path);
		}
		else if (name == "SyzygyProbeDepth")
		{
			syzygy.depth = value::minmax(stoi(value), 1, lim::depth);
		}
		else if (name == "SyzygyProbeLimit")
		{
			syzygy.pieces = value::minmax(stoi(value), 0, lim::syzygy_pieces);
		}
		else if (name == "Syzygy50MoveRule")
		{
			syzygy.rule50 = value::boolean(value);
		}
	}

	void position(std::istringstream &stream, board &pos)
	{
		// reacting to the 'position' command

		std::string token, fen;
		stream >> token;

		if (token == "startpos")
		{
			fen = startpos;
			stream >> token;
		}
		else if (token == "fen")
		{
			while (stream >> token && token != "moves")
				fen += token + " ";
		}

		set_position(pos, fen);
		if (token != "moves")
			return;

		// executing the move sequence

		while (stream >> token)
			game::new_move(pos, move::encode(pos, token));
	}

	void show_options(board &pos)
	{
		// reacting to the 'uci' command

		sync::cout
			<< "id name Monolith " << version_number
			<< "\nid author Jonas Mayr\n"

			<< "\noption name Threads type spin default " << uci::thread_count << " min 1 max " << lim::threads
			<< "\noption name Ponder type check default " << value::boolean(ponder)
			<< "\noption name Hash type spin default " << hash_size << " min 1 max " << lim::hash
			<< "\noption name Clear Hash type button"
			<< "\noption name UCI_Chess960 type check default " << value::boolean(chess960)
			<< "\noption name MultiPV type spin default " << multipv << " min 1 max " << lim::multipv
			<< "\noption name Contempt type spin default " << contempt[pos.turn]
			<< " min " << lim::min_contempt << " max " << lim::max_contempt
			<< "\noption name Move Overhead type spin default " << overhead << " min 0 max " << lim::overhead

			<< "\noption name OwnBook type check default " << value::boolean(use_book)
			<< "\noption name Book File type string default " << book::name

			<< "\noption name SyzygyPath type string default " << syzygy.path
			<< "\noption name SyzygyProbeDepth type spin default " << syzygy.depth << " min 1 max " << lim::depth
			<< "\noption name SyzygyProbeLimit type spin default " << syzygy.pieces << " min 0 max " << lim::syzygy_pieces
			<< "\noption name Syzygy50MoveRule type check default " << value::boolean(syzygy.rule50)

			<< "\nuciok" << std::endl;
	}
}

void uci::open_book()
{
	// connecting to the PolyGlot opening book

	bookmove = book::open();
}

void uci::set_position(board &pos, std::string fen)
{
	// setting a new position

	pos.parse_fen(fen);
	game::reset();
	assert(uci::move_offset == 0);
	quiet_hash[uci::move_offset] = pos.key;
}

void uci::new_game(board &pos)
{
	// responding to the 'ucinewgame' command

	bookmove = use_book;
	hash::table.clear();
	search::reset();
	set_position(pos, startpos);
}

void uci::loop()
{
	// initializing board & thread objects

	std::string input, token;
	board pos;
	new_game(pos);
	thread_pool threads(thread_count, pos);
	threads.start_all();

	do // communication loop
	{
		std::getline(std::cin, input);
		std::istringstream stream(input);
		stream >> token;

		if (input == "uci")
		{
			show_options(pos);
		}
		else if (input == "stop")
		{
			threads.stop_search();
			infinite = false;
		}
		else if (input == "ponderhit")
		{
			infinite = false;
		}
		else if (input == "isready")
		{
			sync::cout << "readyok" << std::endl;
		}
		else if (input == "ucinewgame")
		{
			if (!stop) continue;
			threads.stop_search();
			new_game(pos);
		}
		else if (token == "setoption")
		{
			if (!stop) continue;
			threads.stop_search();
			setoption(stream, threads, pos);
		}
		else if (token == "position")
		{
			if (!stop) continue;
			threads.stop_search();
			position(stream, pos);
		}
		else if (token == "go")
		{
			if (!stop) continue;
			threads.stop_search();
			go(stream, threads, pos);
		}

		// unofficial commands for debugging

		else if (token == "bench")
		{
			// running a benchmark consisting of various search positions
			// 'bench' or 'bench <positions.epd> <time in ms>'

			if (!stop) continue;
			int64 movetime{};
			stream >> token;
			stream >> movetime;
			bench::search(token, movetime);
		}
		else if (token == "perft")
		{
			// running perft on a set of positions
			// 'perft legal' or 'perft pseudo' differentiates the move generation mode

			if (!stop) continue;
			if (!(stream >> token) || token == "legal" || token == "pseudo")
				bench::perft(token);
		}
		else if (input == "eval")
		{
			// doing a itemized evaluation of the current position

			if (!stop) continue;
			pawn hash(false);
			eval::itemise_eval(pos, hash);
		}
		else if (input == "board")
		{
			// displaying the current piece placement

			if (!stop) continue;
			display::position(pos);
		}
		else if (input == "summary")
		{
			// outputting some statistics of the previous search

			if (!stop) continue;
			analysis::summary();
		}

		// unofficial command for tuning if the TUNE switch is on

		else if (token == "tune")
		{
			// running an evaluation parameter tuning session
			// 'tune <quiet-positions.epd> <thread-count>'

			int thread_count{};
			stream >> token;
			stream >> thread_count;
			texel::tune(token, thread_count);
		}

	} while (input != "quit");
}