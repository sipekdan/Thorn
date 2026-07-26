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
#include "movegen.h"
#include "types.h"

class MovePicker {
public:
  MovePicker(const Position& pos, Move tt_move, const Move killers[2] = nullptr, const int history[2][64][64] = nullptr, bool captures_only = false);
  Move next_move();

private:
  const Position& pos;
  Move tt_move;
  Move killers[2];
  const int (*history)[64][64];
  bool captures_only;

  enum Stage { TT_STAGE, GEN_CAPTURES, CAPTURES, GEN_QUIETS, QUIETS, DONE };
  Stage stage;

  MoveList moves;
  int scores[256];
  int current_idx;

  void score_moves();
  Move get_best();
  bool is_valid_move(Move m);
};
