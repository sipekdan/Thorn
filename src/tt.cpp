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

#include <cstring>

#include "tt.h"

namespace TT {
  size_t num_buckets = 0;
  // Bucket* table = nullptr;

  std::unique_ptr<Bucket[]> table = nullptr;

  void clear() {
    if (table) {
      for(size_t i = 0; i < num_buckets; i++) {
        table[i].lock.clear();
        table[i].entries[0] = {0, 0, 0, 0, EXACT};
        table[i].entries[1] = {0, 0, 0, 0, EXACT};
        table[i].entries[2] = {0, 0, 0, 0, EXACT};
      }
    }
  }

  void resize(int mb) {
    // if (table) delete[] table;
    // size_t bytes = (size_t)mb * 1024ULL * 1024ULL;
    // num_buckets = bytes / sizeof(Bucket);
    // if (num_buckets > 0) {
    //   table = new Bucket[num_buckets]();
    // }
    
    size_t bytes = (size_t)mb * 1024ULL * 1024ULL;
    num_buckets = bytes / sizeof(Bucket);
    
    if (num_buckets > 0) {
      // make_unique cleanly allocates and zero-initializes the array
      table = std::make_unique<Bucket[]>(num_buckets);
    } else {
      table.reset();
    }
  }

  void store(U64 key, int depth, int score, Flag flag, Move best_move, int ply) {
    if (num_buckets == 0) return;
    size_t index = key % num_buckets;
    Bucket& b = table[index];

    // Spinlock to ensure thread-safety
    while (b.lock.test_and_set(std::memory_order_acquire));

    if (score > MATE_SCORE - 100) score += ply;
    else if (score < -MATE_SCORE + 100) score -= ply;

    bool saved = false;

    for (int i = 0; i < 3; i++) {
      if (b.entries[i].key == key) {
        if (depth >= b.entries[i].depth) {
            b.entries[i] = {key, best_move, score, (uint8_t)depth, flag};
        }
        saved = true;
        break;
      }
    }

    // 2. If it's a completely new position
    if (!saved) {
      // Does it deserve the protected "Deep" slot?
      if (depth >= b.entries[0].depth) {
        b.entries[0] = {key, best_move, score, (uint8_t)depth, flag};
      } 
      // Otherwise, toss it into the shallower of the two "Always Replace" slots
      else {
        if (b.entries[1].depth <= b.entries[2].depth) {
          b.entries[1] = {key, best_move, score, (uint8_t)depth, flag};
        } else {
          b.entries[2] = {key, best_move, score, (uint8_t)depth, flag};
        }
      }
    }

    b.lock.clear(std::memory_order_release);
  }

  bool probe(U64 key, int depth, int alpha, int beta, int ply, int &return_score, Move &tt_move) {
    if (num_buckets == 0) return false;
    size_t index = key % num_buckets;
    Bucket& b = table[index];

    // Spinlock to safely read the entry
    while (b.lock.test_and_set(std::memory_order_acquire));
    Entry e;
    bool found = false;
    
    for (int i = 0; i < 3; i++) {
      if (b.entries[i].key == key) {
        e = b.entries[i];
        found = true;
        break;
      }
    }
    b.lock.clear(std::memory_order_release);

    if (found) {
      tt_move = e.best_move;
      if (e.depth >= depth) {
        int score = e.score;
        if (score > MATE_SCORE - 100) score -= ply;
        else if (score < -MATE_SCORE + 100) score += ply;

        if (e.flag == EXACT) { return_score = score; return true; }
        if (e.flag == LOWER && score >= beta) { return_score = beta; return true; }
        if (e.flag == UPPER && score <= alpha) { return_score = alpha; return true; }
      }
    }
    return false;
  }

  int hashfull() {
    if (num_buckets == 0) return 0;
    int count = 0;
    int max_check = std::min((int)num_buckets, 1000);
    
    for (int i = 0; i < max_check; i++) {
      while (table[i].lock.test_and_set(std::memory_order_acquire));
      // A bucket is counted as used if ANY of its 3 slots are filled
      if (table[i].entries[0].key != 0 || table[i].entries[1].key != 0 || table[i].entries[2].key != 0) {
        count++;
      }
      table[i].lock.clear(std::memory_order_release);
    }
    return (count * 1000) / max_check;
  }
}