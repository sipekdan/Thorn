/*
  Thorn, a UCI chess engine
  Copyright (C) 2026 Daniel Sipek

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "zobrist.h"

namespace Zobrist {
  U64 pieces[2][6][64];
  U64 castling[16];
  U64 ep[8];
  U64 side;

  void init() {
    U64 seed = 1070372;
    auto rand64 = [&seed]() {
      seed ^= seed >> 12;
      seed ^= seed << 25;
      seed ^= seed >> 27;
      return seed * 2685821657736338717ULL;
    };

    for (int c = 0; c < 2; c++) {
      for (int p = 0; p < 6; p++) {
        for (int sq = 0; sq < 64; sq++)
          pieces[c][p][sq] = rand64();
      }
    }

    for (int i = 0; i < 8; i++)
      ep[i] = rand64();
    for (int i = 0; i < 16; i++)
      castling[i] = rand64();
    side = rand64();
  }
}
