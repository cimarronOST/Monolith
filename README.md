Monolith 0.2

Copyright (C) 2017 Jonas Mayr

This program is distributed under the GNU General Public License. Please read LICENSE for more information.


About:
- Monolith is a small open source UCI-compliant chess-engine written in C++14 by a not too busy medicin student who prefers to invest his extra time into computer chess rather than to seek better grades, because it's simply much more fun. Huge credits go to the Chess Programming Wiki chessprogramming.wikispaces.com and to the equally fantastic talkchess.com. Without them this little project would have never been possible.


Features:
- move-generator with magic bitboards
- alpha-beta-search with some pruning/extending
- reasonable move ordering
- transposition table
- simple hand-tuned evaluation
- support for PolyGlot opening books


Useful informations:
- If your computer does not support POPCNT and LZCNT, use the *_noSSE4 binaries.
- I haven't compiled any 32-bit-executables.
- The standard PolyGlot openingbook of Monolith is called 'book.bin' and has to be contained in the same directory as the binary file in order to be used.
- Only the Windows-binaries support PolyGlot books.


Contact:
- cimarronOST@gmail.com
