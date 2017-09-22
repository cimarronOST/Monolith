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


#include "bench.h"
#include "logfile.h"
#include "engine.h"
#include "uci.h"

namespace
{
	void stop_thread_if(std::thread &searching)
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
			if(searching.joinable())
				searching.join();
			return false;
		}
		else
			return true;
	}

	void infinite(chronos &chrono)
	{
		engine::depth = lim::depth;
		engine::nodes = lim::nodes;
		chrono.movetime = lim::movetime;
		chrono.moves_to_go = 50;
	}

	bool to_bool(std::string value)
	{
		return value == "true" ? true : false;
	}

	std::string to_bool_str(bool value)
	{
		return value == true ? "true" : "false";
	}

	uint32 to_move(const pos &board, std::string input)
	{
		// encoding the coordinate-movestring

		char promo{ input.back() };
		if (input.size() == 5) input.pop_back();

		assert(input.size() == 4);

		auto sq1{ to_idx(input.substr(0, 2)) };
		auto sq2{ to_idx(input.substr(2, 2)) };

		uint8 flag{ NONE };
		auto piece{ board.piece_sq[sq1] };
		auto victim{ board.piece_sq[sq2] };

		if (piece == PAWNS)
		{
			// enpassant flag

			if (victim == NONE && abs(sq1 - sq2) % 8 != 0)
			{
				flag = ENPASSANT;
				victim = PAWNS;
			}

			// doublepush flag

			if (abs(sq1 - sq2) == 16)
				flag = DOUBLEPUSH;

			// promotion flags

			if      (promo == 'q') flag = PROMO_QUEEN;
			else if (promo == 'r') flag = PROMO_ROOK;
			else if (promo == 'b') flag = PROMO_BISHOP;
			else if (promo == 'n') flag = PROMO_KNIGHT;
		}

		// castling flags

		else if (piece == KINGS)
		{
			if      (sq1 == E1 && sq2 == G1) flag = CASTLING::WHITE_SHORT;
			else if (sq1 == E8 && sq2 == G8) flag = CASTLING::BLACK_SHORT;
			else if (sq1 == E1 && sq2 == C1) flag = CASTLING::WHITE_LONG;
			else if (sq1 == E8 && sq2 == C8) flag = CASTLING::BLACK_LONG;
		}
		else
			assert(piece > PAWNS && piece < KINGS);

		return encode(sq1, sq2, flag, piece, victim, board.turn);
	}
}

void uci::search(pos *board, chronos *chrono)
{
	// retrieveing a book move, or (if there is none) starting the search

	engine::stop = false;

	uint32 ponder{ 0 };
	uint32 best_move
	{
		engine::use_book && engine::get_book_move(*board)
		? engine::get_book_move(*board)
		: engine::start_searching(*board, *chrono, ponder)
	};

	assert(best_move != NO_MOVE);
	engine::stop = true;

	log::cout << "bestmove " << algebraic(best_move);

	if (ponder) log::cout << " ponder " << algebraic(ponder);
	log::cout << std::endl;
}

void uci::go(std::istringstream &stream, std::thread &searching, pos &board, chronos &chrono)
{
	// applying specifications before starting the search

	std::string token;

	infinite(chrono);

	while (stream >> token)
	{
		if (token == "movestogo")
		{
			stream >> chrono.moves_to_go;
		}
		else if (token == "wtime")
		{
			stream >> chrono.time[WHITE];
			chrono.movetime = 0;
		}
		else if (token == "btime")
		{
			stream >> chrono.time[BLACK];
			chrono.movetime = 0;
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
		}

		// special cases

		else if (token == "depth")
		{
			stream >> engine::depth;
			if (engine::depth > lim::depth)
				engine::depth = lim::depth;
		}
		else if (token == "nodes")
		{
			stream >> engine::nodes;
		}
		else if (token == "movetime")
		{
			stream >> token;
			chrono.set_movetime(stoi(token));
		}
		else if (token == "infinite")
		{
			engine::infinite = true;
		}
	}

	searching = std::thread{ search, &board, &chrono };
}

void uci::setoption(std::istringstream &stream)
{
	// parsing the setoption command

	std::string token, name, value;
	stream >> name, stream >> name, stream >> token;

	while (token != "value")
	{
		name += " " + token;
		if (!(stream >> token)) break;
	}
	stream >> value;

	// setting the new option
	
	if (name == "Hash")
	{
		int new_hash{ stoi(value) };
		engine::new_hash_size(new_hash <= lim::hash ? new_hash : lim::hash);
	}
	else if (name == "Clear Hash")
	{
		engine::clear_hash();
	}
	else if (name == "Contempt")
	{
		int new_cont{ stoi(value) };

		if (new_cont < lim::min_cont) new_cont = lim::min_cont;
		if (new_cont > lim::max_cont) new_cont = lim::max_cont;

		engine::contempt = new_cont;
	}
	else if (name == "Ponder")
	{
	}
	else if (name == "OwnBook")
	{
		engine::use_book = to_bool(value);
	}
	else if (name == "Best Book Line")
	{
		engine::best_book_line = to_bool(value);
	}
	else if (name == "Book File")
	{
		engine::new_book(value);
	}
}

void uci::position(std::istringstream &stream, pos &board)
{
	// setting up a new position

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

	engine::parse_fen(board, fen);

	if (token != "moves") return;

	// executing the move sequence

	while (stream >> token)
		engine::new_move(board, to_move(board, token));
}

void uci::options()
{
	// responding the "uci" command

	std::cout
		<< "id name Monolith " << version
		<< "\nid author Jonas Mayr\n"

		<< "\noption name Ponder type check default false"
		<< "\noption name OwnBook type check default " << to_bool_str(engine::use_book)
		<< "\noption name Book File type string default " << engine::get_book_name()
		<< "\noption name Best Book Line type check default " << to_bool_str(engine::best_book_line)
		<< "\noption name Hash type spin default " << engine::hash_size << " min 1 max " << lim::hash
		<< "\noption name Clear Hash type button"
		<< "\noption name Contempt type spin default " << engine::contempt
		<< " min " << lim::min_cont << " max " << lim::max_cont

		<< "\nuciok" << std::endl;
}

void uci::loop()
{
	// initialising variables

	std::string input, token;
	pos board;
	chronos chrono;
	std::thread searching;

	engine::new_game(board);
	engine::init_book();

	// communication loop

	do
	{
		std::getline(std::cin, input);

		std::istringstream stream(input);
		stream >> token;

		if (input == "uci")
		{
			options();
		}
		else if (input == "stop")
		{
			stop_thread_if(searching);
			engine::infinite = false;
		}
		else if (input == "ponderhit")
		{
			engine::stop_ponder();
		}
		else if (input == "isready")
		{
			std::cout << "readyok" << std::endl;
		}
		else if (input == "ucinewgame")
		{
			if (ignore(searching))
				continue;
			engine::new_game(board);
		}
		else if (token == "setoption")
		{
			if (ignore(searching))
				continue;
			setoption(stream);
		}
		else if (token == "position")
		{
			if (ignore(searching))
				continue;
			position(stream, board);
		}
		else if (token == "go")
		{
			if (ignore(searching))
				continue;
			go(stream, searching, board, chrono);
		}

		// unofficial commands for debugging

		else if (input == "bench")
		{
			bench::search();
		}
		else if (token == "perft")
		{
			if(!(stream >> token) || token == "legal" || token == "pseudolegal")
				bench::perft(token);
		}
		else if (input == "eval")
		{
			engine::eval(board);
		}

	} while (input != "quit");

	stop_thread_if(searching);
}
