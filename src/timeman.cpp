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

#include <algorithm>
#include <cmath>

#include "timeman.h"
#include "position.h"
#include "types.h"

namespace TimeMan {
  int calculate_time(const Position& pos, int wtime, int btime, int winc, int binc, int movetime, int movestogo) {
    if (movetime > 0) return std::max(10, movetime - 50); 

    int time_left = (pos.side_to_move == WHITE) ? wtime : btime;
    int inc = (pos.side_to_move == WHITE) ? winc : binc;

    if (time_left <= 0) return -1; // Infinite time / fixed depth mode

    double base_time;
    if (movestogo > 0) {
      base_time = time_left / (double)(movestogo + 2); 
    } else {
      base_time = time_left / 40.0;
    }

    // Apply a Gaussian curve that peaks at move 35
    double move_factor = 0.8 + 0.5 * std::exp(-0.003 * std::pow(pos.fullmove_number - 35, 2));

    double opt_time = (base_time * move_factor) + (inc * 0.75);

    // Hard upper limits to completely prevent flagging
    double max_time_proportion = time_left * 0.20;
    double max_time_absolute = time_left - 100.0;

    double allocated = std::min({opt_time, max_time_proportion, max_time_absolute});

    return std::max(10, (int)allocated);
  }
}
