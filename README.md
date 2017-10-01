# Monolith 0.3

This program is distributed under the GNU General Public License.
Please read LICENSE for more information.

Monolith is a open source UCI-compliant chess engine written in C++14 by a not too busy medicin student who prefers to invest his extra time into computer chess rather than to seek better grades.
It's simply much more fun.

Monolith is not a standalone chess programm, but needs a GUI in order to be used properly, for example Arena (www.playwitharena.com).
Huge credits go to the Chess Programming Wiki (https://chessprogramming.wikispaces.com) and to the equally fantastic www.talkchess.com.
Without them this little project would have never been possible.


## Main features
- staged pseudo legal move-generation with magic bitboards
- alphabeta-search with most of the standard enhancements
- basic move ordering
- transposition table with 4 buckets
- still simple hand-tuned evaluation
- internal support for PolyGlot opening books


## Strenght
Monolith 0.3 64-bit achieved 2402 Elo on the CCRL 40/4 (www.computerchess.org.uk/ccrl/404).
That's a gain of about +170 Elo to the previous version.


## Main changes to the previous version
- added Principal Variation Search
- added Reverse Futility Pruning
- added SEE for move-ordering
- added SEE pruning in quiescence search
- new and improved search extensions
- higher rook-on-open-file bonus
- a lot of small evaluation tweaks
- transposition table improvements
- new staged and pseudolegal move generator (sadly a Elo loss)
- completely rewritten UCI handling
- added new important UCI options
- fixed the reported bug that prevented the use of external opening books
- added support to use internal books under Linux
- a lot of other bug fixes and improvements


## Which executable to use
- **x64** is the fastest
- **x64_no_popcnt** does not use the POPCNT- and LZCNT-instructions. Therefore it runs also on older computers, but is a bit slower
- **x86** is relatively slow because there is no x86-dedicated code


## Have fun
That's what it's all about.
