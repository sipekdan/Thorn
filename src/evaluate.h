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

#include "position.h"
#include "nnue.h"

namespace Eval {

  void init();
  int evaluate(const Position& pos/*, const NNUE::Accumulator& acc*/);

  struct Score {
    int mg, eg;
    Score operator+(const Score& o) const { return {mg + o.mg, eg + o.eg}; }
    Score operator-(const Score& o) const { return {mg - o.mg, eg - o.eg}; }
    Score& operator+=(const Score& o) { mg += o.mg; eg += o.eg; return *this; }
    Score& operator-=(const Score& o) { mg -= o.mg; eg -= o.eg; return *this; }
  };

  // Pre-defined here for tuning
  // TODO: Remove this after tuning done (and add const in evaluate.cpp)
  extern Score pawn_table[64];
  extern Score knight_table[64];
  extern Score bishop_table[64];
  extern Score rook_table[64];
  extern Score queen_table[64];
  extern Score king_table[64];

  extern Score piece_value[6];

  extern Score passed_pawn_bonus[8];
  extern Score BISHOP_PAIR;
  extern Score ROOK_SEMI_OPEN;
  extern Score ROOK_OPEN;
  extern int   TEMPO_BONUS;
}
