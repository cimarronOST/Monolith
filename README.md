Monolith 0.1

Copyright (C) 2017 Jonas Mayr.

This program is distributed under the GNU General Public License. Please read LICENSE for more information.


About:
- Monolith is a small open source UCI-compliant chess-engine written in C++14 by a not too busy medicin student who prefers to invest his extra time into computer chess rather than to seek better grades, because it's simply much more fun. This version of Monolith is the first one of a hopefully long and prosper development process. Huge credits go to the Chess Programming Wiki, an incredibly valuable resorce. Without it this little project would have never been possible.


Features:
- reasonable move-generator with magic bitboards
- alpha-beta-search with some hand tuned pruning/extending
- some basic move ordering
- a transposition table which worsens the playing strenght
- simple evaluation, consisting only of material- and PSQT-evaluation
- support for PolyGlot opening books


Useful informations:
- If your computer does not support POPCNT and LZCNT, use the noSSE4 executable files.
- I haven't compiled any 32-bit-compartible files.
- Note that the files for Linux are considerably slower than those for Windows.
- The standard PolyGlot openingbook of Monolith is called 'book.bin' and has to be contained in the same directory as the executional file in order to be used.


Contact:
- cimarronOST@gmail.com
