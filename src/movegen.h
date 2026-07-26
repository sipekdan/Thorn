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

#include "types.h"
#include "position.h"
#include "bitboard.h"

namespace Movegen {
  void generate_pseudo_moves(const Position &pos, MoveList &moves);
  void generate_legal_moves(const Position &pos, MoveList &moves);

  void generate_tactical(const Position &pos, MoveList &moves);

  bool is_square_attacked(const Position &pos, int sq, int attacker_color);
  bool is_in_check(const Position &pos, int color);
}
