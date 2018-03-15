/*
  Monolith 0.4  Copyright (C) 2017 Jonas Mayr

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


#include "chronos.h"
#include "notation.h"
#include "movegen.h"
#include "bench.h"
#include "stream.h"
#include "engine.h"
#include "uci.h"

namespace thread
{
	void stop_if(std::thread &searching)
	{
		if (searching.joinable())
		{
			engine::stop = true;
			searching.join();
		}
	}

	bool ignore(std::thread &searching)
	{
		if (engine::stop)
		{
			if (searching.joinable())
				searching.join();
			return false;
		}
		else
			return true;
	}
}

namespace limits
{
	void infinite()
	{
		engine::limit.moves.clear();
		engine::limit.nodes = lim::nodes;
		engine::limit.depth = lim::depth;
		engine::limit.mate = 0;
	}
}

namespace value
{
	bool boolean(std::string value)
	{
		return value == "true" ? true : false;
	}

	std::string boolean(bool value)
	{
		return value == true ? "true" : "false";
	}
}

namespace move
{
	uint32 encode(board &pos, std::string input)
	{
		// encoding the coordinate movestring

		gen list(pos, LEGAL);
		list.gen_all();

		for (auto i{ 0 }; i < list.cnt.moves; ++i)
		{
			if (input == notation::algebraic(list.move[i]))
				return list.move[i];
		}
		return NO_MOVE;
	}
}

namespace display
{
	void position(board &pos)
	{
		const std::string row{ "+---+---+---+---+---+---+---+---+" };
		sync::cout << row << "\n";
		for (int sq{ A8 }; sq >= H1; --sq)
		{
			auto piece{ "PNBRQK  "[pos.piece_sq[sq]] };
			sync::cout << "| " << ((1ULL << sq) & pos.side[BLACK] ? static_cast<char>(tolower(piece)) : piece) << " ";
			if (sq % 8 == 0)
				sync::cout << "|\n" << row << std::endl;
		}
	}
}

void uci::search(board *pos, uint64 time)
{
	// retrieveing a book move, or (if there is none) starting the search

	engine::stop = false;
	uint32 ponder{ };
	uint32 best_move{ engine::book_move ? engine::get_book_move(*pos) : NO_MOVE };

	best_move = { best_move ? best_move : engine::start_searching(*pos, time, ponder) };
	assert(best_move != NO_MOVE);
	engine::stop = true;

	sync::cout << "bestmove " << notation::algebraic(best_move)
		<< (ponder ? " ponder " + notation::algebraic(ponder) : "")
		<< std::endl;
}

void uci::searchmoves(std::istringstream &stream, board &pos)
{
	// parsing the 'go searchmoves' command

	std::string token;
	while (stream >> token)
	{
		auto move{ move::encode(pos, token) };
		if (move == NO_MOVE)
		{
			std::string remains{ token };
			while (stream >> token)
				remains += " " + token;

			stream.str(remains);
			stream.clear();
			break;
		}
		engine::limit.moves.push_back(move);
	}
}

void uci::go(std::istringstream &stream, std::thread &searching, board &pos)
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
			engine::infinite = true;
			chrono.infinite = false;
		}

		// special cases

		else if (token == "searchmoves")
		{
			searchmoves(stream, pos);
		}
		else if (token == "nodes")
		{
			stream >> engine::limit.nodes;
		}
		else if (token == "depth")
		{
			stream >> engine::limit.depth;
			engine::limit.depth = std::min(engine::limit.depth, lim::depth);
		}
		else if (token == "mate")
		{
			stream >> engine::limit.mate;
		}
		else if (token == "movetime")
		{
			stream >> chrono.movetime;
		}
		else if (token == "infinite")
		{
			engine::infinite = true;
		}
	}

	searching = std::thread{ search, &pos, chrono.get_movetime(pos.turn) };
}

void uci::setoption(std::istringstream &stream, board &pos)
{
	// parsing the 'setoption' command

	std::string token, name, value;
	stream >> name;
	stream >> name;

	while (stream >> token && token != "value")
		name += " " + token;
	stream >> value;

	// setting the new option
	
	if (name == "Hash")
	{
		auto new_hash{ stoi(value) };
		engine::new_hash_size(std::min(new_hash, lim::hash));
	}
	else if (name == "Clear Hash")
	{
		engine::clear_hash();
	}
	else if (name == "Contempt")
	{
		auto contempt{ stoi(value) };
		contempt = std::min(contempt, lim::max_contempt);
		contempt = std::max(contempt, lim::min_contempt);

		engine::contempt[pos.turn]  =  contempt;
		engine::contempt[pos.xturn] = -contempt;
	}
	else if (name == "Ponder")
	{
		engine::ponder = value::boolean(value);
	}
	else if (name == "Move Overhead")
	{
		engine::overhead = std::min(stoi(value), lim::overhead);
	}
	else if (name == "UCI_Chess960")
	{
		engine::chess960 = value::boolean(value);
	}
	else if (name == "MultiPV")
	{
		engine::multipv = std::min(stoi(value), lim::multipv);
	}
	else if (name == "OwnBook")
	{
		engine::use_book  = value::boolean(value);
		engine::book_move = value::boolean(value);
	}
	else if (name == "Best Book Line")
	{
		engine::best_book_line = value::boolean(value);
	}
	else if (name == "Book File")
	{
		engine::new_book(value);
	}
}

void uci::position(std::istringstream &stream, board &pos)
{
	// reacting to the 'position' command

	std::string token, fen;
	stream >> token;

	if (token == "startpos")
	{
		fen = engine::startpos;
		stream >> token;
	}
	else if (token == "fen")
	{
		while (stream >> token && token != "moves")
			fen += token + " ";
	}

	engine::set_position(pos, fen);
	if (token != "moves")
		return;

	// executing the move sequence

	while (stream >> token)
		engine::new_move(pos, move::encode(pos, token));
}

void uci::show_options(board &pos)
{
	// reacting to the 'uci' command

	sync::cout
		<< "id name Monolith " << version_number
		<< "\nid author Jonas Mayr\n"

		<< "\noption name Ponder type check default " << value::boolean(engine::ponder)
		<< "\noption name OwnBook type check default " << value::boolean(engine::use_book)
		<< "\noption name Book File type string default " << engine::get_book_name()
		<< "\noption name Best Book Line type check default " << value::boolean(engine::best_book_line)
		<< "\noption name Hash type spin default " << engine::hash_size << " min 1 max " << lim::hash
		<< "\noption name Clear Hash type button"
		<< "\noption name UCI_Chess960 type check default " << value::boolean(engine::chess960)
		<< "\noption name MultiPV type spin default " << engine::multipv << " min 1 max " << lim::multipv
		<< "\noption name Contempt type spin default " << engine::contempt[pos.turn]
		<< " min " << lim::min_contempt << " max " << lim::max_contempt
		<< "\noption name Move Overhead type spin default " << engine::overhead << " min 0 max " << lim::overhead

		<< "\nuciok" << std::endl;
}

void uci::loop()
{
	// initialising variables

	std::string input, token;
	board pos;
	std::thread searching;
	engine::new_game(pos);

	// communication loop

	do
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
			thread::stop_if(searching);
			engine::infinite = false;
		}
		else if (input == "ponderhit")
		{
			engine::infinite = false;
		}
		else if (input == "isready")
		{
			sync::cout << "readyok" << std::endl;
		}
		else if (input == "ucinewgame")
		{
			if (thread::ignore(searching))
				continue;
			engine::new_game(pos);
		}
		else if (token == "setoption")
		{
			if (thread::ignore(searching))
				continue;
			setoption(stream, pos);
		}
		else if (token == "position")
		{
			if (thread::ignore(searching))
				continue;
			position(stream, pos);
		}
		else if (token == "go")
		{
			if (thread::ignore(searching))
				continue;
			go(stream, searching, pos);
		}

		// unofficial commands for debugging

		else if (input == "bench")
		{
			bench::search();
		}
		else if (token == "perft")
		{
			if(!(stream >> token) || token == "legal" || token == "pseudo")
				bench::perft(token);
		}
		else if (input == "eval")
		{
			engine::eval(pos);
		}
		else if (input == "board")
		{
			display::position(pos);
		}
		else if (input == "summary")
		{
			engine::search_summary();
		}

	} while (input != "quit");

	thread::stop_if(searching);
}