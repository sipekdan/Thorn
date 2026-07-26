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

#include <cstdint>

#include "types.h"

struct Position;

namespace NNUE {

    constexpr int INPUT_SIZE = 768;  // 12 pieces * 64 squares
    constexpr int HIDDEN_SIZE = 256; // 256 neurons

    struct Accumulator {
        alignas(64) int16_t white[HIDDEN_SIZE];
        alignas(64) int16_t black[HIDDEN_SIZE];
    };

    struct Network {
        int16_t feature_weights[INPUT_SIZE][HIDDEN_SIZE];
        int16_t feature_bias[HIDDEN_SIZE];
        int16_t output_weights[HIDDEN_SIZE * 2]; 
        int16_t output_bias;
    };

    extern Network net;
    extern bool is_loaded;

    bool load_network(const char* filepath);
    int evaluate(int side_to_move, const Accumulator& acc);

    // Accumulator Management
    void refresh_accumulator(const Position& pos, Accumulator& acc);
    Accumulator get_next_accumulator(const Position& pos, Move m, const Accumulator& acc);
}