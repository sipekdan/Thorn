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

#include <iostream>
#include <chrono>
#include <vector>
#include <thread>
#include <atomic>
#include <climits>

#include "perft.h"
#include "movegen.h"
#include "threads.h"

namespace Perft {
  U64 perft(Position& pos, int depth) {
    if (depth == 0) return 1ULL;

    MoveList moves;
    Movegen::generate_legal_moves(pos, moves);

    if (depth == 1) return moves.count;

    U64 nodes = 0;
    for (int i = 0; i < moves.count; i++) {
      UndoInfo undo = pos.make_move(moves.moves[i]);
      nodes += perft(pos, depth - 1);
      pos.unmake_move(moves.moves[i], undo);
    }
    return nodes;
  }

  uint64_t divide(Position& pos, int depth) {
    auto start = std::chrono::high_resolution_clock::now();
    MoveList moves;
    Movegen::generate_legal_moves(pos, moves);

    std::atomic<U64> total_nodes{0};
    std::atomic<int> current_move_idx{0};

    std::vector<std::atomic<U64>> move_nodes(moves.count);
    for(int i = 0; i < moves.count; i++) move_nodes[i].store(ULLONG_MAX);

    int num_threads = Threads::GLOBAL_THREADS;
    std::vector<std::thread> workers;

    for (int i = 0; i < num_threads; ++i) {
      workers.emplace_back([&, depth]() {
        Position local_pos = pos;
        U64 thread_local_total = 0;
        
        while (true) {
          int idx = current_move_idx.fetch_add(1);
          if (idx >= moves.count) break;

          Move m = moves.moves[idx];
          UndoInfo undo = local_pos.make_move(m);
          
          U64 nodes = 0;
          // if (!Movegen::is_in_check(local_pos, local_pos.side_to_move ^ 1)) {
          //   nodes = perft(local_pos, depth - 1);
          // }
          nodes = perft(local_pos, depth - 1);

          local_pos.unmake_move(m, undo);

          thread_local_total += nodes;
          move_nodes[idx].store(nodes);
        }

        total_nodes.fetch_add(thread_local_total, std::memory_order_relaxed);
      });
    }

    for (int i = 0; i < moves.count; i++) {
      while (move_nodes[i].load() == ULLONG_MAX) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
      if (move_nodes[i].load() > 0) {
        std::cout << move_to_string(moves.moves[i]) << ": " << move_nodes[i].load() << "\n";
      }
    }

    for (auto& t : workers) { if (t.joinable()) t.join(); }

    auto end = std::chrono::high_resolution_clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "\nNodes: " << total_nodes
              << "\nTime:  " << diff.count() << " ms"
              << std::endl;

    return total_nodes;
  }
}
