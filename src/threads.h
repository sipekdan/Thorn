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
#include <chrono>
#include <vector>
#include <thread>

#include "position.h"

namespace Threads {
  extern int GLOBAL_THREADS;
  extern std::atomic<bool> stop_flag;
  extern int search_time_limit;
  extern std::chrono::time_point<std::chrono::high_resolution_clock> start_time;
  extern std::atomic<uint64_t> global_nodes;

  void start_search(Position& pos, int target_depth, int time_limit_ms);
  bool check_time();
  int64_t get_elapsed_ms();
}
