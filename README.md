# Monolith 0.4

This program is distributed under the GNU General Public License.
Please read LICENSE for more information.

Monolith is a open source UCI-compliant chess engine written in C++11 by a not too busy medicin student
who prefers to invest his extra time into computer chess rather than to seek better grades.
It's simply much more fun.

Monolith is not a standalone chess programm, but needs a GUI in order to be used properly,
for example [Arena](http://www.playwitharena.com) or [Cute Chess](https://github.com/cutechess/cutechess).
Huge credits go to the [Chess Programming Wiki](https://chessprogramming.wikispaces.com) and to the equally fantastic [www.talkchess.com](http://www.talkchess.com).
Without them this little project would have never been possible.


## Main features
- staged pseudo-legal move-generation with magic bitboards
- alphabeta-search with most of the common enhancements
- full Universal Chess Interface (UCI) protocol support
- internal support for PolyGlot opening books
- support for Fischer Random Chess


## Strenght
Monolith 0.4 achieved almost 2600 Elo on both [CCRL 40/4](http://www.computerchess.org.uk/ccrl/404/index.html) and [CCRL 40/40](http://www.computerchess.org.uk/ccrl/4040/index.html).
That's an improvement of about 180 Elo over Monolith 0.3.


## Main changes to the previous version
- added support for Fischer Random Chess
- added support for the UCI 'MultiPV' option
- added pawn hash table
- added check- and evasion-generation in quiescence search
- improved Null Move pruning
- added Internal Iterative Deepening
- added Late Move Pruning
- huge number of other small additions, improvements and bug-fixes


## Which executable to use
- **x64** is the fastest.
- **x64_no_popcnt** does not need modern CPU instruction sets, therefore runs also on older computers, but is a bit slower.
- **x86** runs also on 32-bit computers, but is considerably slower because there is no dedicated code for 32-bit instruction sets.


### If you compile it yourself
A makefile is included. Running the 'bench' command should result in a total of **40813238** nodes.


## UCI options overview
- **Ponder**: Searching also during the opponents turn. Default is set to false.
- **OwnBook**: Making use of a PolyGlot opening book internally if available. Default is set to true.
- **Book File**: Specifying the location in which the PolyGlot opening book is placed. Default path is set to the same location as the engine, the default book name is 'monolith.bin'.
- **Best Book Line**: Always choosing the best move from the opening book. Default is set to false in order to play more diverse games, but even with false set, higher rated book moves are choosen more likely.
- **Hash**: Specifying the size of the transposition hash table. Default is set to 128 MB.
- **Clear Hash**: Clearing the hash table, e.g. to be able to start searching whithout being unaffected by previous hash entries.
- **UCI_Chess960**: Choosing to play the chess variant Chess 960 / Fischer Random Chess. Default is set to false.
- **MultiPV**: Specifying the number of principal variation line output. Default is set to 1.
- **Contempt**: Setting the level of the internal draw score. A positive contempt means that draws are being avoided in order to play more aggressive and go for the win. A negative contempt means that draws are being pursued in order to play safe and less risky. Default is set to 0.
- **Move Overhead**: Adding a time buffer for every move if communications between GUI and engine are delayed, in order to avoid time losses. Default is set to 0 milliseconds.


### Additional unofficial commands
- 'bench': Running a benchmark consisting of a set of positions.
- 'perft legal': Running a performance test on some positions, using the legal move-generator.
- 'perft pseudo': Running the same performance test using the pseudo-legal move-generator.
- 'eval': Outputting the detailed static evaluation of the current position.
- 'board': Showing the board.
- 'summary': Showing some statistics about the previous search.


### Known issues
- For analysis of mate-in-n positions, the score and PV may not always be correct.


## Have fun
That's what it's all about.
