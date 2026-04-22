# Monolith

[![C++](https://img.shields.io/badge/c++-%2300599C.svg?style=for-the-badge&logo=c%2B%2B&logoColor=white)](#)
[![license](https://img.shields.io/github/license/cimarronOST/Monolith?style=for-the-badge&color=blue)](https://github.com/cimarronOST/Monolith/blob/master/LICENSE)
[![release](https://img.shields.io/github/v/release/cimarronOST/Monolith?style=for-the-badge&color=blue)](https://github.com/cimarronOST/Monolith/releases/latest)

Monolith is a classical chess engine that uses a highly optimized alpha-beta search algorithm in combination with a handcrafted evaluation function. Both the search and evaluation parameters are automatically tuned using logistic regression and stochastic approximation, respectively.
In addition to experimenting with further enhancements to continuously increase playing strength, Monolith thrives towards a clean codebase and generally being a fun project to work on. The code is annotated with Elo estimates to gauge the impact of each implemented feature on playing strength.
Monolith is compliant with the Universal Chess Interface (UCI) protocol and can be run via any UCI-compatible interface.


## Elo rating
The approximate strength as tested by rating lists:

| Release | Estimation | [CCRL Blitz](https://www.computerchess.org.uk/ccrl/404) | [CCRL 40/15](https://www.computerchess.org.uk/ccrl/4040) | [CEGT 40/20](http://www.cegt.net/40_40%20Rating%20List/40_40%20BestVersion/rangliste.html)
| ------------ | ---- | ---- | ---- | ---- |
| Monolith 3   | 3200 | 3260 | 3250 | 3110 |
| Monolith 2   | 3000 | 3060 | 3040 | 2910 |
| Monolith 1   | 2800 | 2810 | 2800 | 2660 |
| Monolith 0.4 | 2600 | 2600 | 2600 |      |
| Monolith 0.3 | 2400 | 2410 | 2390 |      |
| Monolith 0.2 | 2200 | 2240 |      |      |
| Monolith 0.1 | 2000 |      |      |      |


## Main features
- Move-generation: magic bitboards and PEXT bitboards
- Evaluation: handcrafted, tuned with logistic regression (Texel tuning method)
- Search: alpha-beta algorithm, tuned with stochastic approximation (SPSA)
- Support for:
  - Universal Chess Interface (UCI) protocol
  - Fischer Random Chess / Chess960
  - Multiple CPU threads
  - [Syzygy endgame table-bases](https://github.com/syzygy1/tb) created by Ronald de Man


## Precompiled executables

| Platform          | Description |
| ----------------- | --- |
| **x86-64-pext**   | making use of the BMI2 PEXT instruction of recent CPUs |
| **x86-64**        | does not need modern CPU instruction sets |
| **armv64**        | for Apple silicon CPUs |
| **armv8**         | targets ARM AArch64 and works on most Android devices |
| **armv7**         | targets ARM AArch32 and runs also on old Android devices |


## Compilation instructions
Simply run `make` which will compile Monolith optimized for the building machine.\
Running the Monolith `bench` command should result in a total of `42858236` nodes.

Further options: `make [ARCH=architecture] [COMP=compiler]`
- targetable platform architectures, see above for more detailed descriptions:\
`x86-64-pext`, `x86-64`, `armv64`, `armv8`, `armv7`;
- tested compilers:\
`g++` `clang++` `icpx`


## UCI options overview
- **`Threads`**: Number of CPU threads that are available to be used in parallel. Default is `1`.
- **`Ponder`**: Continuing to search for the next move during the opponents turn (as humans do when playing chess). Default is `false`.
- **`Hash`**: Size of the Transposition Hash Table which speeds up the search and makes parallel search with multiple threads much more efficient. Default is `128` MB.
- **`Clear Hash`**: Clearing the Transposition Hash Table. This can be used to start a new search without being affected by previously saved search results.
- **`UCI_Chess960`**: Adhering to the rules of the chess variant Fischer Random Chess / Chess960. Default is `false`.
- **`MultiPV`**: Number of best moves and their variations to be displayed in detail. Default is `1`. A higher value can be useful for analyzing positions but significantly reduces the engine's overall playing strength since the search effort is spread across multiple moves.
- **`Move Overhead`**: Time buffer to be used if the communication between interface and engine is delayed, in order to avoid time losses. Default is `0` milliseconds.
- **`Log`**: Redirecting all output of the engine to a log file called monolith_log.txt. Default is `false`.
- **`SyzygyPath`**: Location of the Syzygy endgame table-bases. Default is `<empty>`. Multiple paths should be separated with a semicolon (`;`) on Windows and with a colon (`:`) on Linux.
- **`SyzygyProbeDepth`**: Limiting the use of the table-bases to nodes which have a reasonable search depth remaining. Default is set to `5`, a higher value should be used if the search speed of the engine drops a lot because of slow table-base access.


#### Additional unofficial commands
- `bench`: Running benchmark searches of an internal set of various positions.
- `speedtest`: Running `bench` multiple times, useful to test the speed of the engine.
- `perft [depth]`: Running perft up to [depth] on the current position.
- `eval`: Computing the static evaluation of the current position without the use of the search function.
- `board`: Displaying a basic character-chessboard of the current position.


## Acknowledgements
The [Chess Programming Wiki](https://www.chessprogramming.org) and the community on the [Computer Chess Club](http://www.talkchess.com) offered 
an endless source of inspiration while writing this engine.
I'm also grateful for the [CCRL](http://www.computerchess.org.uk) group for including the engine in their rating lists since the very start of development.\
Some of the ideas incorporated into Monolith derive from the king of all chess engines, [Stockfish](https://github.com/official-stockfish/Stockfish),
so thanks to all the people involved in this engine for pushing the limits and making their ideas open source.
It's becoming increasingly difficult to pinpoint the origin of the bazillion recent optimizations and improvements in
computer chess, since most modern engines share a lot of their basic composition and have similar implementation details.
So credits are due to all the chess programmers before me who raised the ceiling and made their ideas publicly available.


## Have fun
That's what it's all about.
