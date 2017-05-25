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


#include "console.h"

#ifdef DEBUG

#include <sstream>
#include <thread>

#include "hash.h"
#include "files.h"
#include "search.h"
#include "game.h"
#include "movegen.h"
#include "engine.h"
#include "convert.h"
#include "benchmark.h"

namespace
{
	void stop_thread_if(std::thread &searching, pos &board)
	{
		if (searching.joinable())
		{
			engine::stop = true;
			searching.join();
			console::game_end(board);
		}
	}
	int move_nr(pos &board)
	{
		return static_cast<int>((board.moves - 1) / 2) + 1;
	}

	int state;
}

void console::search(pos *board, chronos *chrono)
{
	engine::stop = false;

	auto best_move{ engine::alphabeta(*board, *chrono) };

	assert(best_move != 0);

	uint64 sq1{ 1ULL << to_sq1(best_move) };
	uint64 sq2{ 1ULL << to_sq2(best_move) };
	uint8 flag{ to_flag(best_move) };
	
	string move_str{ conv::bit_to_san(*board, sq1, sq2, flag) };
	
	engine::new_move(*board, sq1, sq2, flag);

	print(*board);
	log::cout << move_nr(*board) << "." << move_str << endl;
}

void console::loop()
{
	log::cout << "CONSOLE MODE LOADED" << endl;

	string input, token;
	pos board;
	chronos chrono;
	std::thread searching;

	while (std::getline(std::cin, input))
	{
		std::istringstream stream(input);
		stream >> token;

		if (input == "newgame")
		{
			stop_thread_if(searching, board);

			engine::new_game(board, chrono);

			print(board);
			state = ACTIVE;
		}
		else if (input == "go")
		{
			stop_thread_if(searching, board);

			if (engine::use_book && engine::get_book_move(board))
			{
				uint16 move{ engine::get_book_move(board) };

				uint64 sq1{ 1ULL << to_sq1(move) };
				uint64 sq2{ 1ULL << to_sq2(move) };
				uint8 flag{ to_flag(move) };

				string move_str{ conv::bit_to_san(board, sq1, sq2, flag) };
				engine::new_move(board, sq1, sq2, flag);

				print(board);
				log::cout << "bookmove\n" << move_nr(board) << "." << move_str << endl;
			}
			else
			{
				searching = std::thread{ search, &board, &chrono };
			}
		}
		else if (token == "set")
		{
			stop_thread_if(searching, board);

			while (stream >> token)
			{
				if (token == "depth")
				{
					stream >> token;
					engine::depth = stoi(token);
					if (engine::depth > lim::depth)
						engine::depth = lim::depth;
					log::cout << "depth set to <" << engine::depth << ">" << endl;
				}
				else if (token == "movetime")
				{
					stream >> token;
					chrono.set_movetime(stoi(token));
					log::cout << "movetime set to <" << token << ">" << endl;
				}
				else if (token == "book")
				{
					stream >> token;
					if (token == "true")
					{
						engine::use_book = true;
						log::cout << "openingbook set to <true>" << endl;
					}
					else if (token == "false")
					{
						engine::use_book = false;
						log::cout << "openingbook set to <false>" << endl;
					}
				}
				else if (token == "hash")
				{
					if (stream >> token)
					{
						engine::new_hash_size(stoi(token));
						log::cout << "hash set to " << engine::hash_size << " MB" << endl;
					}
				}
				else std::cerr << "set \'<option> <value>\'" << endl;
			}
		}
		else if (token == "position")
		{
			stop_thread_if(searching, board);

			stream >> token;
			string fen;
			if (token == "fen")
			{
				while (stream >> token)
					fen += token + " ";

				engine::parse_fen(board, chrono, fen);
				chrono.only_movetime = true;

				print(board);
			}
			else if (token == "startpos")
			{
				engine::new_game(board, chrono);
				print(board);
			}
			else
			{
				std::cerr << "position \'startpos\'" << endl;
				std::cerr << "position \'fen <fen>\'" << endl;
			}
		}
		else if (token == "perft")
		{
			stop_thread_if(searching, board);

			if (stream >> token)
			{
				int depth{ stoi(token) };
				analysis::root_perft(board, 1, depth);
			}
			else std::cerr << "perft \'<depth>\'" << endl;
		}
		else if (token == "bench")
		{
			stop_thread_if(searching, board);

			if (input == "bench full") stream.str("perft search");

			while (stream >> token)
				benchmark::analysis(token);
		}
		else if (token == "exit" || token == "quit")
		{
			stop_thread_if(searching, board);
			break;
		}
		else if (token == "stop")
		{
			stop_thread_if(searching, board);
		}

		// move input

		else if (conv::san_to_move(board, input))
		{
			stop_thread_if(searching, board);

			uint16 move{ conv::san_to_move(board, input) };
			
			movegen gen(board, ALL);

			if(gen.in_list(move))
			{
				uint64 sq1{ 1ULL << to_sq1(move) };
				uint64 sq2{ 1ULL << to_sq2(move) };
				uint8 flag{ to_flag(move)};

				string move_str{ conv::bit_to_san(board, sq1, sq2, flag) };
				engine::new_move(board, sq1, sq2, flag);

				print(board);
				log::cout << move_nr(board) << "." << move_str << endl;
				if (game_end(board))
					break;
			}
			else std::cerr << "error: invalid move" << endl;
		}
		else
		{
			if (game_end(board))
				break;

			if (!input.empty())
				std::cerr << "error: unknown command" << endl;
		}
	}
}

bool console::game_end(pos &board)
{
	update_state(board);

	switch (state)
	{
	case ACTIVE:
		return false;
	case CHECKMATE:
		log::cout << "# CHECKMATE" << endl;
		std::cin.get();
		return true;
	case STALEMATE:
		log::cout << "1/2-1/2 STALEMATE" << endl;
		std::cin.get();
		return true;
	case ISDRAW:
		log::cout << "1/2-1/2 DRAW" << endl;
		std::cin.get();
		return true;
	default:
		assert(false);
		return false;
	}
}
void console::update_state(pos &board)
{
	movegen gen(board, ALL);

	if (!gen.move_cnt)
	{
		if (gen.check(board, board.turn, board.side[board.turn] & board.pieces[KINGS]))
			state = STALEMATE;
		else
			state = CHECKMATE;
	}
	else if (draw::verify(board, game::hashlist, 0))
		state = ISDRAW;
	else
		state = ACTIVE;
}
void console::to_char(const pos &board, char board_char[])
{
	const char p_char[][8]
	{
		{ 'P', 'R', 'N', 'B', 'Q', 'K' },
		{ 'p', 'r', 'n', 'b', 'q', 'k' }
	};

	for (int col{ WHITE }; col <= BLACK; ++col)
	{
		uint64 pieces{ board.side[col] };
		while (pieces)
		{
			auto sq{ engine::bitscan(pieces) };
			auto piece{ board.piece_sq[sq] };

			board_char[sq] = p_char[col][piece];
			pieces &= pieces - 1;
		}
	}
}
void console::print(const pos &board)
{
	char board_char[64]{ 0 };
	to_char(board, board_char);

	log::cout << "\n+---+---+---+---+---+---+---+---+\n";
	for (int a{ 0 }; a < 8; ++a)
	{
		for (int b{ 0 }; b < 8; ++b)
			log::cout << "| " << board_char[63 - a * 8 - b] << " ";
		log::cout << "|\n+---+---+---+---+---+---+---+---+\n";
	}
}

#endif
