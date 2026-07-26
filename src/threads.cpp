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

#include "threads.h"
#include "search.h"

namespace Threads {
  int GLOBAL_THREADS = 1;
  std::atomic<bool> stop_flag{false};
  int search_time_limit = -1;
  std::chrono::time_point<std::chrono::high_resolution_clock> start_time;
  std::atomic<uint64_t> global_nodes{0};

  int64_t get_elapsed_ms() {
    auto now = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();
  }

  bool check_time() {
    if (search_time_limit > 0 && get_elapsed_ms() >= search_time_limit) {
      stop_flag.store(true, std::memory_order_relaxed);
    }
    return stop_flag.load(std::memory_order_relaxed);
  }

  void start_search(Position& pos, int target_depth, int time_limit_ms) {
    stop_flag.store(false);
    search_time_limit = time_limit_ms;
    global_nodes.store(0);
    start_time = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> workers;

    // Spawn helper threads
    for (int i = 1; i < GLOBAL_THREADS; i++) {
      workers.emplace_back([pos, target_depth, i]() {
        // Each thread gets its OWN local copy of the position
        Position local_pos = pos; 
        // Slight depth offset prevents threads from searching the exact same tree
        Search::search_position(local_pos, target_depth + (i % 2), false);
      });
    }

    // The main thread searches too
    Position main_pos = pos;
    Search::search_position(main_pos, target_depth, true);

    // Tell helpers to stop if the main thread finished first, and wait for them
    stop_flag.store(true);
    for (auto& t : workers) {
      if (t.joinable()) t.join();
    }
  }
}
