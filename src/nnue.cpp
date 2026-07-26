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

#include <fstream>
#include <algorithm>
#include <cmath>

#include "nnue.h"
#include "position.h"
#include "bitboard.h"

namespace NNUE {

    Network net;
    bool is_loaded = false;

    bool load_network(const char* filepath) {
        std::ifstream file(filepath, std::ios::binary);
        if (!file.is_open()) return false;

        uint32_t magic;
        file.read(reinterpret_cast<char*>(&magic), sizeof(magic));

        if (magic != 0x4E4E5545) {
            return false;
        }

        file.read(reinterpret_cast<char*>(&net), sizeof(Network));
        is_loaded = true;
        return true;
    }

    inline int get_feature_index(int piece, int color, int sq) {
        return (color * 384) + (piece * 64) + sq;
    }

    inline void add_feature(Accumulator& acc, int piece, int color, int sq) {
        int w_idx = get_feature_index(piece, color, sq);
        int b_idx = get_feature_index(piece, color ^ 1, sq ^ 56); // Flip for black
        for (int i = 0; i < HIDDEN_SIZE; i++) {
            acc.white[i] += net.feature_weights[w_idx][i];
            acc.black[i] += net.feature_weights[b_idx][i];
        }
    }

    inline void remove_feature(Accumulator& acc, int piece, int color, int sq) {
        int w_idx = get_feature_index(piece, color, sq);
        int b_idx = get_feature_index(piece, color ^ 1, sq ^ 56);
        for (int i = 0; i < HIDDEN_SIZE; i++) {
            acc.white[i] -= net.feature_weights[w_idx][i];
            acc.black[i] -= net.feature_weights[b_idx][i];
        }
    }

    // Fully recalculates the accumulator (Used once at the root node)
    void refresh_accumulator(const Position& pos, Accumulator& acc) {
        for (int i = 0; i < HIDDEN_SIZE; i++) {
            acc.white[i] = net.feature_bias[i];
            acc.black[i] = net.feature_bias[i];
        }
        for (int c = WHITE; c <= BLACK; c++) {
            for (int p = PAWN; p <= KING; p++) {
                U64 bb = pos.piece_bb[c][p];
                while (bb) {
                    add_feature(acc, p, c, Bitboard::pop_lsb(bb));
                }
            }
        }
    }

    // Clones the accumulator and updates it based on the move (Blazing fast!)
    Accumulator get_next_accumulator(const Position& pos, Move m, const Accumulator& acc) {
        Accumulator next = acc;
        int us = pos.side_to_move, them = us ^ 1;
        int from = move_from(m), to = move_to(m);
        int piece = pos.board_sq[from], captured = pos.board_sq[to], prom = move_prom(m);

        remove_feature(next, piece, us, from);

        if (prom != NONE) add_feature(next, prom, us, to);
        else add_feature(next, piece, us, to);

        if (captured != NONE) {
            remove_feature(next, captured, them, to);
        } else if (piece == PAWN && to == pos.ep_square) {
            remove_feature(next, PAWN, them, to + (us == WHITE ? -8 : 8)); // En-passant
        }

        // Handle Castling Rook Updates
        if (piece == KING && std::abs(from - to) == 2) {
            if (to == 6)      { remove_feature(next, ROOK, WHITE, 7);  add_feature(next, ROOK, WHITE, 5); }
            else if (to == 2) { remove_feature(next, ROOK, WHITE, 0);  add_feature(next, ROOK, WHITE, 3); }
            else if (to == 62){ remove_feature(next, ROOK, BLACK, 63); add_feature(next, ROOK, BLACK, 61); }
            else if (to == 58){ remove_feature(next, ROOK, BLACK, 56); add_feature(next, ROOK, BLACK, 59); }
        }

        return next;
    }

    // The Neural Network Forward Pass
    // int evaluate(int side_to_move, const Accumulator& acc) {
    //     int32_t output = 0;
    //     const int16_t* us = (side_to_move == WHITE) ? acc.white : acc.black;
    //     const int16_t* them = (side_to_move == WHITE) ? acc.black : acc.white;

    //     for (int i = 0; i < HIDDEN_SIZE; i++) {
    //         int16_t us_act = std::max((int16_t)0, us[i]);     // ReLU
    //         int16_t them_act = std::max((int16_t)0, them[i]); // ReLU
    //         output += us_act * net.output_weights[i];
    //         output += them_act * net.output_weights[HIDDEN_SIZE + i];
    //     }
    //     return (int)(((int64_t)(output + net.output_bias) * 400) / 16384);
    // }

    int evaluate(int side_to_move, const Accumulator& acc) {
        const int16_t* us = (side_to_move == WHITE) ? acc.white : acc.black;
        const int16_t* them = (side_to_move == WHITE) ? acc.black : acc.white;
        
        const int16_t* weights_us = net.output_weights;
        const int16_t* weights_them = net.output_weights + HIDDEN_SIZE;

        int32_t sum = 0;
        
        // Force the compiler to unroll and auto-vectorize this loop
        #pragma GCC unroll 8
        for (int i = 0; i < HIDDEN_SIZE; i++) {
            int16_t us_act = us[i] > 0 ? us[i] : 0;
            int16_t them_act = them[i] > 0 ? them[i] : 0;
            
            sum += us_act * weights_us[i];
            sum += them_act * weights_them[i];
        }
        
        return (int)(((int64_t)(sum + net.output_bias) * 400) / 16384);
    }
}