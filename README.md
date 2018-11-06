# Monolith 1.0

This program is distributed under the GNU General Public License.
Please read LICENSE for more information.

Monolith is an open source UCI-compliant chess engine written in C++11 by a not too busy medical student
who prefers to spend his time messing with computer chess rather than to seek better grades.
It's simply much more fun.

Monolith is not a standalone chess program, but needs a GUI in order to be used properly,
for example the freely available [Arena](http://www.playwitharena.com), [Cute Chess](https://github.com/cutechess/cutechess) or [Tarrash](https://www.triplehappy.com).
Big thanks go to the [Chess Programming Wiki](https://www.chessprogramming.org) and to the equally fantastic [talkchess.com](http://www.talkchess.com)-community which offer
an endless source of chess-programming-wisdom and inspiration, to the [CCRL](http://www.computerchess.org.uk/ccrl) group and the [CEGT](http://www.cegt.net) team for including the engine in their rating lists,
and to all the open-source chess-engines out there which never fail to offer a helpful insight when all seems lost.
Special thanks also go to Tom Kerrigan who provided a lot of information about his [simplified ABDADA](http://www.tckerrigan.com/Chess/Parallel_Search/Simplified_ABDADA) SMP-algorithm,
to Ronald de Man for providing his [syzygy ETB probing code](https://github.com/syzygy1/tb), and to [grzegoszwu](https://www.deviantart.com/grzegoszwu/art/Tulkas-battlecry-613671743) for the picture.


## Main features
- move-generation: staged, pseudo-legal, using fancy-magic or PEXT bitboards
- search-algorithm: alpha-beta- & principal-variation-search
- evaluation: hand crafted, tuned automatically with the Texel-tuning-method
- support for:
  - Universal Chess Interface (UCI) protocol
  - multiple CPU-cores through simplified-ABDADA
  - PolyGlot opening-books
  - Syzygy endgame-tablebases
  - Fischer Random Chess / Chess960


## Strength
Even for the best human Grandmasters it should be very difficult to beat Monolith running even on slow hardware.
Monolith 1.0 achieved about 2800 Elo on both [CCRL 40/4](http://www.computerchess.org.uk/ccrl/404/index.html) and [CCRL 40/40](http://www.computerchess.org.uk/ccrl/4040/index.html).


## Main changes to the previous version
- [x] added support for multi-processors with simplified-ABDADA
- [x] added support for syzygy endgame-tablebases
- [x] added support for PEXT instruction
- [x] added new search enhancements (SEE pruning, singular extension, LMR, aspiration window)
- [x] added new evaluation terms
- [x] tuned the evaluation automatically with Texel's tuning method
- [x] simplified the code
- [x] fixed a bug that caused crashes while moving backwards through the game


## Which pre-compiled executable to use
- **x64_pext** is the fastest, making use of the PEXT instruction which only works with recent CPUs (Intel Haswell and newer).
- **x64_popcnt** is almost as fast, making use of the POPCNT instruction which is not supported by older CPUs.
- **x64** does not need modern CPU instruction sets and therefore runs a bit slower, but works also on older computers (requiring only a 64-bit architecture).
- **x86** runs also on 32-bit systems, but is considerably slower because there is no dedicated code for 32-bit instruction-sets.


#### Compile it yourself
Simply run ```make``` if you don't want to be bothered with optimized processor instructions.

For more options, run ```make [target] [ARCH=architecture] [COMP=compiler]```
- supported targets:
  - ```release``` (default): standard optimized build.
  - ```release_log```: same build but with all engine output redirected to a log-file.
  - ```tune```: build with enabled ability to tune the evaluation.
- supported architectures:
  - ```x64``` (default)
  - ```x64-pext```
  - ```x64-popcnt```
  - ```x86```
- supported compilers:
  - ```clang``` (default): Clang C++ compiler
  - ```gcc```: GNU C++ compiler
  - ```icc```: Intel C++ compiler

> Running the Monolith ```bench``` command should result in a total of ```33143855``` nodes if compiled correctly.


## UCI options overview
- **Threads**: Specifying the number of CPU cores that can be used in parallel. Default is set to 1.
- **Ponder**: Searching also during the opponents turn. Default is set to false.
- **OwnBook**: Giving the engine access to a PolyGlot opening book. Default is set to true, i.e. if the book specified by **Book File** (see below) is found, it will be used.
- **Book File**: Resetting the location in which the PolyGlot opening book is placed. Default is set to the same location as the engine, the default book name is 'monolith.bin'.
- **Hash**: Specifying the size of the transposition hash table. Default is set to 128 MB.
- **Clear Hash**: Clearing the hash table (to be able to start searching without being affected by previous hash entries).
- **UCI_Chess960**: Playing the chess variant Fischer Random Chess / Chess960. Default is set to false.
- **MultiPV**: Specifying the number of the engines best variations to be displayed in detail. Default is set to 1.
- **Contempt**: Resetting the level of the internal draw score. A positive contempt value means that draws are being avoided in order to play more aggressively and risky. A negative contempt value means that draws are being pursued more likely in order to play safely and less risky. Default is set to 0.
- **Move Overhead**: Adding a time buffer for every move if communications between GUI and engine are delayed, in order to avoid time losses. Default is set to 0 milliseconds.
- **SyzygyPath**: Specifying the location of the folders containing the Syzygy tablebases. Default is set to <empty>. Multiple paths should be separated with a semicolon (;) on Windows and with a colon (:) on Linux.
- **SyzygyProbeDepth**: Limiting the use of the tablebases to nodes which remaining depth is higher than SyzygyProbeDepth. Default is set to 5, i.e. the tablebases are used until the remaining depth of a node is 5 or smaller. A higher value is more cache-friendly and should be used if nps drop a lot because of tablebase-probing, a smaller value might lead to better play on computers where tablebases can be accessed very fast.
- **SyzygyProbeLimit**: Limiting the number of pieces that have to be on the board before the tablebases are probed. Default is set to 6 which is the upper limit of pieces that Syzygy tablebases currently cover.
- **Syzygy50MoveRule**: Considering the 50-move-rule while probing Syzygy tablebases. Default is set to true. Setting it to false can be useful to analyze cursed wins or blessed losses.


### Additional unofficial commands
- ```bench```: Running an internal benchmark consisting of a fixed set of positions.
- ```bench [positions.epd] [time]```: Running a benchmark with positions in EPD format from an external file for the time specified in milliseconds.
- ```perft```: Running a performance test on some positions using legal move-generation.
- ```perft pseudo```: Running the same performance test using pseudo-legal move-generation.
- ```eval```: Outputting the detailed static evaluation of the current position.
- ```board```: Displaying the current chess-board.
- ```summary```: Displaying some statistics about the previous search.
- ```tune [quiet-positions.epd]```: Tuning evaluation parameters using quiet positions if compilation-target ```tune``` was enabled.


## Have fun
That's what it's all about.
