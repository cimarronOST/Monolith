/*
  Monolith 0.1  Copyright (C) 2017 Jonas Mayr

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


///#undef GTHR_ACTIVE_PROXY
#include <thread>
#include <sstream>

#include "files.h"
#include "engine.h"
#include "console.h"
#include "convert.h"
#include "position.h"
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
}

void uci::search(pos *board, chronos *chrono)
{
	engine::stop = false;

	auto best_move{ engine::alphabeta(*board, *chrono) };

	assert(best_move != 0);

	uint64 sq1{ 1ULL << to_sq1(best_move) };
	uint64 sq2{ 1ULL << to_sq2(best_move) };
	uint8 flag{ to_flag(best_move) };

	engine::new_move(*board, sq1, sq2, flag);

	log::cout << "bestmove "
		<< conv::to_str(sq1)
		<< conv::to_str(sq2)
		<< conv::to_promo(flag)
		<< endl;
}

void uci::loop()
{
	string input, token;
	pos board;
	chronos chrono;
	std::thread searching;

	do
	{
		std::getline(std::cin, input);

		std::istringstream stream(input);
		stream >> token;

#ifdef DEBUG

		if (input == "console")
		{
			console::loop();
			log::cout << endl;
			break;
		}

#endif

		if (input == "uci")
		{
			std::cout
				<< "id name Monolith " << version << "\n"
				<< "id author Jonas Mayr\n\n"
				<< "option name OwnBook type check default " << (engine::play_with_book ? "true\n" : "false\n")
				<< "option name Hash type spin default " << engine::hash_size << " min 1 max " << lim::hash << "\n"

				<< "option name Futility Pruning type check default false\n"
				<< "option name Ponder type check default false\n"
				<< "option name UCI_Chess960 type check default false\n"
				<< "option name UCI_ShowCurrLine type check default false\n";
			std::cout
				<< "uciok" << endl;
		}
		else if (input == "stop")
		{
			stop_thread_if(searching);
		}
		else if (input == "isready")
		{
			std::cout << "readyok" << endl;
		}
		else if (input == "ucinewgame")
		{
			stop_thread_if(searching);

			engine::new_game(board, chrono);
		}
		else if (token == "setoption")
		{
			stop_thread_if(searching);

			stream >> token;
			string name;
			if (stream >> name && name == "hash")
			{
				stream >> token;
				if(stream >> token)
					engine::init_hash(stoi(token));
			}
		}
		else if (token == "position")
		{
			stop_thread_if(searching);

			stream >> token;
			string fen;
			if (token == "startpos")
			{
				if (stream.peek() == EOF)
				{
					engine::new_game(board, chrono);
					continue;
				}
			}
			else if (token == "fen")
			{
				while (stream >> token && token != "moves")
					fen += token + " ";

				if (token != "moves")
				{
					engine::parse_fen(board, chrono, fen);
					continue;
				}
				else if (stream >> token && stream.peek() == EOF)
				{
					engine::parse_fen(board, chrono, fen);
				}
			}

			while (stream.peek() != EOF)
				stream >> token;

			char promo{ token.back() };
			if (promo == 'q' || promo == 'r' || promo == 'n' || promo == 'b')
				token.pop_back();

			
			assert(token.size() == 4);

			uint64 sq1{ conv::to_bb(token.substr(0, 2)) };
			uint64 sq2{ conv::to_bb(token.substr(2, 2)) };
			uint8 flag{ conv::to_flag(promo, board, sq1, sq2) };

			engine::new_move(board, sq1, sq2, flag);
		}
		else if (token == "go")
		{
			while (stream >> token)
			{
				if (token == "infinite")
				{
					engine::depth = lim::depth;
					chrono.set_movetime(lim::movetime);
				}
				else if (token == "depth")
				{
					stream >> token;
					engine::depth = stoi(token);
					if (engine::depth > lim::depth)
						engine::depth = lim::depth;
				}
				else if (token == "movetime")
				{
					stream >> token;
					chrono.set_movetime(stoi(token));
				}
				else if (token == "wtime")
				{
					stream >> token;
					chrono.time[white] = stoi(token);
					chrono.only_movetime = false;
				}
				else if (token == "btime")
				{
					stream >> token;
					chrono.time[black] = stoi(token);
					chrono.only_movetime = false;
				}
				else if (token == "winc")
				{
					stream >> token;
					chrono.incr[white] = stoi(token);
					chrono.only_movetime = false;
				}
				else if (token == "binc")
				{
					stream >> token;
					chrono.incr[black] = stoi(token);
					chrono.only_movetime = false;
				}
				else if (token == "movestogo")
				{
					stream >> token;
					chrono.moves_to_go = stoi(token);
					chrono.only_movetime = false;
				}
			}
			if (engine::play_with_book && engine::get_book_move(board))
			{
				uint16 move{ engine::get_book_move(board) };

				uint64 sq1{ 1ULL << to_sq1(move) };
				uint64 sq2{ 1ULL << to_sq2(move) };
				uint8 flag{ to_flag(move) };

				engine::new_move(board, sq1, sq2, flag);

				log::cout
					<< "info string move from openingbook\n"
					<< "bestmove "
					<< conv::to_str(sq1)
					<< conv::to_str(sq2)
					<< conv::to_promo(flag)
					<< endl;
			}
			else
			{
				searching = std::thread{ search, &board, &chrono };
			}
		}

	} while (input != "quit");

	//// cleaning up
	stop_thread_if(searching);
	engine::delete_hash();
}