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

#pragma once

#include <atomic>
#include <memory>

#include "types.h"

namespace TT {
  enum Flag : uint8_t { EXACT, LOWER, UPPER };

  struct Entry {
    U64 key;
    Move best_move;
    int score;
    uint8_t depth;
    Flag flag;
  };

  struct Bucket {
    Entry entries[3];
    std::atomic_flag lock = ATOMIC_FLAG_INIT;
  };

  void resize(int mb);
  void clear();
  void store(U64 key, int depth, int score, Flag flag, Move best_move, int ply);
  bool probe(U64 key, int depth, int alpha, int beta, int ply, int &score, Move &tt_move);

  int hashfull();
}