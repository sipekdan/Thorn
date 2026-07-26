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

// #include "evaluate.h"

// namespace Eval {
//   const int piece_values[6] = {100, 320, 330, 500, 900, 20000};

//   void init() {}

//   int evaluate(const Position& pos, const NNUE::Accumulator& acc) {
//     if (NNUE::is_loaded) {
//       return NNUE::evaluate(pos.side_to_move, acc);
//     }

//     int white_mat = 0;
//     int black_mat = 0;

//     for (int p = PAWN; p <= KING; p++) {
//       white_mat += Bitboard::popcount(pos.piece_bb[WHITE][p]) * piece_values[p];
//       black_mat += Bitboard::popcount(pos.piece_bb[BLACK][p]) * piece_values[p];
//     }

//     int score = white_mat - black_mat;
//     return (pos.side_to_move == WHITE) ? score : -score;
//   }
// }

#include <algorithm>

#include "evaluate.h"
#include "position.h"
#include "bitboard.h"

namespace Eval {
  // struct Score {
  //   int mg, eg;
  //   Score operator+(const Score& o) const { return {mg + o.mg, eg + o.eg}; }
  //   Score operator-(const Score& o) const { return {mg - o.mg, eg - o.eg}; }
  //   Score& operator+=(const Score& o) { mg += o.mg; eg += o.eg; return *this; }
  //   Score& operator-=(const Score& o) { mg -= o.mg; eg -= o.eg; return *this; }
  // };

  #define S(mg, eg) {mg, eg}

  // const int mg_value[6] = {82, 337, 365, 477, 1025, 0};
  // const int eg_value[6] = {94, 281, 297, 512, 936, 0};
  /*const */ Score piece_value[6] = { S(82, 94), S(337, 281), S(365, 297), S(477, 512), S(1025, 936), S(0, 0) };

  // Score piece_value[6] = { S(62, 63), S(293, 269), S(345, 297), S(433, 468), S(981, 892), S(0, 0) };

  /// Phase weights determine how "complex" the position is. Used to interpolate smoothly.
  const int phase_weights[6] = {0, 1, 1, 2, 4, 0};
  const int MAX_PHASE = 24;

  // const int BISHOP_PAIR_MG = 30;
  // const int BISHOP_PAIR_EG = 50;
  // const int ROOK_SEMI_OPEN_MG = 15;
  // const int ROOK_OPEN_MG = 30;
  // const int ROOK_OPEN_EG = 15;
  // const int TEMPO_BONUS = 15;

  /// Positional and tactical bonuses (in centipawns)
  
  // TODO: Add const here
  Score BISHOP_PAIR      = S(30, 50);
  Score ROOK_SEMI_OPEN   = S(15, 0);
  Score ROOK_OPEN        = S(30, 15);
  int   TEMPO_BONUS      = 15;


  // Score BISHOP_PAIR = S(10, 46);
  // Score ROOK_SEMI_OPEN = S(23, 22);
  // Score ROOK_OPEN = S(64, -2);
  // int TEMPO_BONUS = 43;

  // Score OUTPOST_BONUS        = S(25, 15);
  Score MINOR_BEHIND_PAWN    = S(15, 5);
  Score TRAPPED_ROOK_PENALTY = S(40, 15);
  Score THREAT_ON_QUEEN      = S(25, 10);

  // const int passed_pawn_mg[8] = {0, 5, 10, 20, 35, 60, 100, 0};
  // const int passed_pawn_eg[8] = {0, 10, 25, 45, 80, 130, 200, 0};

  /// Passed pawn bonuses scaling by rank (0 to 7)
  /*const  */Score passed_pawn_bonus[8] = { S(0, 0), S(5, 10), S(10, 25), S(20, 45), S(35, 80), S(60, 130), S(100, 200), S(0, 0) };

  // Score passed_pawn_bonus[8] = { S(0, 0), S(5, -2), S(-18, 21), S(-15, 44), S(10, 58), S(60, 123), S(100, 200), S(0, 0) };

  const int center_distance[64] = {
    6, 5, 4, 3, 3, 4, 5, 6,
    5, 4, 3, 2, 2, 3, 4, 5,
    4, 3, 2, 1, 1, 2, 3, 4,
    3, 2, 1, 0, 0, 1, 2, 3,
    3, 2, 1, 0, 0, 1, 2, 3,
    4, 3, 2, 1, 1, 2, 3, 4,
    5, 4, 3, 2, 2, 3, 4, 5,
    6, 5, 4, 3, 3, 4, 5, 6
  };

  const U64 FILE_MASKS[8] = {
    0x0101010101010101ULL, 0x0202020202020202ULL, 0x0404040404040404ULL, 0x0808080808080808ULL,
    0x1010101010101010ULL, 0x2020202020202020ULL, 0x4040404040404040ULL, 0x8080808080808080ULL
  };

  U64 passed_pawn_masks[2][64];
  U64 isolated_pawn_masks[8];

  /// init() precalculates pawn masks for rapid evaluation of pawn structure during the search.
  void init() {
    for (int f = 0; f < 8; f++) {
      U64 file_mask = FILE_MASKS[f];
      isolated_pawn_masks[f] = ((f > 0) ? (file_mask >> 1) : 0) | ((f < 7) ? (file_mask << 1) : 0);
    }
    for (int sq = 0; sq < 64; sq++) {
      int f = sq % 8;
      U64 front_white = 0, front_black = 0;
      for (int r = (sq / 8) + 1; r <= 7; r++)
        front_white |= (0xFFULL << (r * 8));
      for (int r = (sq / 8) - 1; r >= 0; r--)
        front_black |= (0xFFULL << (r * 8));

      U64 adjacent = isolated_pawn_masks[f] | FILE_MASKS[f];
      passed_pawn_masks[WHITE][sq] = front_white & adjacent;
      passed_pawn_masks[BLACK][sq] = front_black & adjacent;
    }
  }

  // const int mg_pawn_table[64] = {
  //   0, 0, 0, 0, 0, 0, 0, 0,
  //   98, 134, 61, 95, 68, 126, 34, -11,
  //   -6, 7, 26, 31, 65, 56, 25, -20,
  //   -14, 13, 6, 21, 23, 12, 17, -23,
  //   -27, -2, -5, 12, 17, 6, 10, -25,
  //   -26, -4, -4, -10, 3, 3, 33, -12,
  //   -35, -1, -20, -23, -15, 24, 38, -22,
  //   0, 0, 0, 0, 0, 0, 0, 0
  // };

  // const int eg_pawn_table[64] = {
  //   0, 0, 0, 0, 0, 0, 0, 0,
  //   178, 173, 158, 134, 147, 132, 165, 187,
  //   94, 100, 85, 67, 56, 53, 82, 84,
  //   32, 24, 13, 5, -2, 4, 17, 17,
  //   13, 9, -3, -7, -7, -8, 3, -1,
  //   4, 7, -6, 1, 0, -5, -1, -8,
  //   13, 8, 8, 10, 13, 0, 2, -7,
  //   0, 0, 0, 0, 0, 0, 0, 0
  // };

  // const int mg_knight_table[64] = {
  //     -167, -89, -34, -49, 61, -97, -15, -107,
  //     -73, -41, 72, 36, 23, 62, 7, -17,
  //     -47, 60, 37, 65, 84, 129, 73, 44,
  //     -9, 17, 19, 53, 37, 69, 18, 22,
  //     -13, 4, 16, 13, 28, 19, 21, -8,
  //     -23, -9, 12, 10, 19, 17, 28, -16,
  //     -29, -53, -12, -3, -1, 18, -14, -19,
  //     -105, -21, -58, -33, -17, -28, -19, -23};
  // const int eg_knight_table[64] = {
  //     -58, -38, -13, -28, -31, -27, -63, -99,
  //     -25, -8, -25, -2, -9, -25, -24, -52,
  //     -24, -20, 10, 9, -1, -9, -19, -41,
  //     -17, 3, 22, 22, 22, 11, 8, -18,
  //     -18, -6, 16, 25, 16, 17, 4, -18,
  //     -23, -3, -1, 15, 10, -3, -20, -22,
  //     -42, -20, -10, -5, -2, -20, -23, -44,
  //     -29, -51, -23, -38, -22, -27, -43, -74};

  // const int mg_bishop_table[64] = {
  //     -29, 4, -82, -37, -25, -42, 7, -8,
  //     -26, 16, -18, -13, 30, 59, 18, -47,
  //     -16, 37, 43, 40, 35, 50, 37, -2,
  //     -4, 5, 19, 50, 37, 37, 7, -2,
  //     -6, 13, 13, 26, 34, 12, 10, 4,
  //     0, 15, 15, 15, 14, 27, 18, 10,
  //     4, 15, 16, 0, 7, 21, 33, 1,
  //     -33, -3, -14, -21, -13, -12, -39, -21};
  // const int eg_bishop_table[64] = {
  //     -14, -21, -11, -8, -7, -9, -17, -24,
  //     -8, -4, 7, -12, -3, -13, -4, -14,
  //     2, -8, 0, -1, -2, 6, 0, 4,
  //     -3, 9, 12, 9, 14, 10, 3, 2,
  //     -6, 3, 13, 19, 7, 10, -3, -9,
  //     -12, -3, 8, 10, 13, 3, -7, -15,
  //     -14, -18, -7, -1, 4, -9, -15, -27,
  //     -23, -9, -23, -5, -9, -16, -5, -17};

  // const int mg_rook_table[64] = {
  //     32, 42, 32, 51, 63, 9, 31, 43,
  //     27, 32, 58, 62, 80, 67, 26, 44,
  //     -5, 19, 26, 36, 17, 45, 61, 16,
  //     -24, -11, 7, 26, 24, 35, -8, -20,
  //     -36, -26, -12, -1, 9, -7, 6, -23,
  //     -45, -25, -16, -17, 3, 0, -5, -33,
  //     -44, -16, -20, -9, -1, 11, -6, -71,
  //     -19, -13, 1, 17, 16, 7, -37, -26};
  // const int eg_rook_table[64] = {
  //     13, 10, 18, 15, 12, 12, 8, 5,
  //     11, 13, 13, 11, -3, 3, 8, 3,
  //     7, 7, 7, 5, 4, -3, -5, -3,
  //     4, 3, 13, 1, 2, 1, -1, 2,
  //     3, 5, 8, 4, -5, -6, -8, -11,
  //     -4, 0, -5, -1, -7, -12, -8, -16,
  //     -6, -6, 0, 2, -9, -9, -11, -3,
  //     -9, 2, 3, -1, -5, -13, 4, -20};

  // const int mg_queen_table[64] = {
  //     -28, 0, 29, 12, 59, 44, 43, 45,
  //     -24, -39, -5, 1, -16, 57, 28, 54,
  //     -13, -17, 7, 8, 29, 56, 47, 57,
  //     -27, -27, -16, -16, -1, 17, -2, 1,
  //     -9, -26, -9, -10, -2, -4, 3, -3,
  //     -14, 2, -11, -2, -5, 2, 14, 5,
  //     -35, -8, 11, 2, 8, 15, -3, 1,
  //     -1, -18, -9, 10, -15, -25, -31, -50};
  // const int eg_queen_table[64] = {
  //     -9, 22, 22, 27, 27, 19, 10, 20,
  //     -17, 20, 32, 41, 58, 25, 30, 0,
  //     -20, 6, 9, 49, 47, 35, 19, 9,
  //     3, 22, 24, 45, 57, 40, 57, 36,
  //     -18, 28, 19, 47, 31, 34, 12, 11,
  //     -16, -27, 15, 6, 9, 17, 10, 5,
  //     -22, -23, -30, -16, -16, -23, -36, -32,
  //     -33, -28, -22, -43, -5, -32, -20, -41};

  // const int mg_king_table[64] = {
  //     -65, 23, 16, -15, -56, -34, 2, 13,
  //     29, -1, -20, -7, -8, -4, -38, -29,
  //     -9, 24, 2, -16, -20, 6, 22, -22,
  //     -17, -20, -12, -27, -30, -25, -14, -36,
  //     -49, -1, -27, -39, -46, -44, -33, -51,
  //     -14, -14, -22, -46, -44, -30, -15, -27,
  //     1, 7, -8, -64, -43, -16, 9, 8,
  //     -15, 36, 12, -54, 8, -28, 24, 14};
  // const int eg_king_table[64] = {
  //     -74, -35, -18, -18, -11, 15, 4, -17,
  //     -12, 17, 14, 17, 17, 38, 23, 11,
  //     10, 17, 23, 15, 20, 45, 44, 13,
  //     -8, 22, 24, 27, 26, 33, 26, 3,
  //     -18, -4, 21, 24, 27, 23, 9, -11,
  //     -19, -3, 11, 21, 23, 16, 7, -9,
  //     -27, -11, 4, 13, 14, 4, -5, -17,
  //     -53, -34, -21, -11, -28, -14, -24, -43};

  Score pawn_table[64] = {
    S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0),
    S(  98,  178), S( 134,  173), S(  61,  158), S(  95,  134), S(  68,  147), S( 126,  132), S(  34,  165), S( -11,  187),
    S(  -6,   94), S(   7,  100), S(  26,   85), S(  31,   67), S(  65,   56), S(  56,   53), S(  25,   82), S( -20,   84),
    S( -14,   32), S(  13,   24), S(   6,   13), S(  21,    5), S(  23,   -2), S(  12,    4), S(  17,   17), S( -23,   17),
    S( -27,   13), S(  -2,    9), S(  -5,   -3), S(  12,   -7), S(  17,   -7), S(   6,   -8), S(  10,    3), S( -25,   -1),
    S( -26,    4), S(  -4,    7), S(  -4,   -6), S( -10,    1), S(   3,    0), S(   3,   -5), S(  33,   -1), S( -12,   -8),
    S( -35,   13), S(  -1,    8), S( -20,    8), S( -23,   10), S( -15,   13), S(  24,    0), S(  38,    2), S( -22,   -7),
    S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0)
  };

  Score knight_table[64] = {
    S(-167,  -58), S( -89,  -38), S( -34,  -13), S( -49,  -28), S(  61,  -31), S( -97,  -27), S( -15,  -63), S(-107,  -99),
    S( -73,  -25), S( -41,   -8), S(  72,  -25), S(  36,   -2), S(  23,   -9), S(  62,  -25), S(   7,  -24), S( -17,  -52),
    S( -47,  -24), S(  60,  -20), S(  37,   10), S(  65,    9), S(  84,   -1), S( 129,   -9), S(  73,  -19), S(  44,  -41),
    S(  -9,  -17), S(  17,    3), S(  19,   22), S(  53,   22), S(  37,   22), S(  69,   11), S(  18,    8), S(  22,  -18),
    S( -13,  -18), S(   4,   -6), S(  16,   16), S(  13,   25), S(  28,   16), S(  19,   17), S(  21,    4), S(  -8,  -18),
    S( -23,  -23), S(  -9,   -3), S(  12,   -1), S(  10,   15), S(  19,   10), S(  17,   -3), S(  28,  -20), S( -16,  -22),
    S( -29,  -42), S( -53,  -20), S( -12,  -10), S(  -3,   -5), S(  -1,   -2), S(  18,  -20), S( -14,  -23), S( -19,  -44),
    S(-105,  -29), S( -21,  -51), S( -58,  -23), S( -33,  -38), S( -17,  -22), S( -28,  -27), S( -19,  -43), S( -23,  -74)
  };

  Score bishop_table[64] = {
    S( -29,  -14), S(   4,  -21), S( -82,  -11), S( -37,   -8), S( -25,   -7), S( -42,   -9), S(   7,  -17), S(  -8,  -24),
    S( -26,   -8), S(  16,   -4), S( -18,    7), S( -13,  -12), S(  30,   -3), S(  59,  -13), S(  18,   -4), S( -47,  -14),
    S( -16,    2), S(  37,   -8), S(  43,    0), S(  40,   -1), S(  35,   -2), S(  50,    6), S(  37,    0), S(  -2,    4),
    S(  -4,   -3), S(   5,    9), S(  19,   12), S(  50,    9), S(  37,   14), S(  37,   10), S(   7,    3), S(  -2,    2),
    S(  -6,   -6), S(  13,    3), S(  13,   13), S(  26,   19), S(  34,    7), S(  12,   10), S(  10,   -3), S(   4,   -9),
    S(   0,  -12), S(  15,   -3), S(  15,    8), S(  15,   10), S(  14,   13), S(  27,    3), S(  18,   -7), S(  10,  -15),
    S(   4,  -14), S(  15,  -18), S(  16,   -7), S(   0,   -1), S(   7,    4), S(  21,   -9), S(  33,  -15), S(   1,  -27),
    S( -33,  -23), S(  -3,   -9), S( -14,  -23), S( -21,   -5), S( -13,   -9), S( -12,  -16), S( -39,   -5), S( -21,  -17)
  };

  Score rook_table[64] = {
    S(  32,   13), S(  42,   10), S(  32,   18), S(  51,   15), S(  63,   12), S(   9,   12), S(  31,    8), S(  43,    5),
    S(  27,   11), S(  32,   13), S(  58,   13), S(  62,   11), S(  80,   -3), S(  67,    3), S(  26,    8), S(  44,    3),
    S(  -5,    7), S(  19,    7), S(  26,    7), S(  36,    5), S(  17,    4), S(  45,   -3), S(  61,   -5), S(  16,   -3),
    S( -24,    4), S( -11,    3), S(   7,   13), S(  26,    1), S(  24,    2), S(  35,    1), S(  -8,   -1), S( -20,    2),
    S( -36,    3), S( -26,    5), S( -12,    8), S(  -1,    4), S(   9,   -5), S(  -7,   -6), S(   6,   -8), S( -23,  -11),
    S( -45,   -4), S( -25,    0), S( -16,   -5), S( -17,   -1), S(   3,   -7), S(   0,  -12), S(  -5,   -8), S( -33,  -16),
    S( -44,   -6), S( -16,   -6), S( -20,    0), S(  -9,    2), S(  -1,   -9), S(  11,   -9), S(  -6,  -11), S( -71,   -3),
    S( -19,   -9), S( -13,    2), S(   1,    3), S(  17,   -1), S(  16,   -5), S(   7,  -13), S( -37,    4), S( -26,  -20)
  };

  Score queen_table[64] = {
    S( -28,   -9), S(   0,   22), S(  29,   22), S(  12,   27), S(  59,   27), S(  44,   19), S(  43,   10), S(  45,   20),
    S( -24,  -17), S( -39,   20), S(  -5,   32), S(   1,   41), S( -16,   58), S(  57,   25), S(  28,   30), S(  54,    0),
    S( -13,  -20), S( -17,    6), S(   7,    9), S(   8,   49), S(  29,   47), S(  56,   35), S(  47,   19), S(  57,    9),
    S( -27,    3), S( -27,   22), S( -16,   24), S( -16,   45), S(  -1,   57), S(  17,   40), S(  -2,   57), S(   1,   36),
    S(  -9,  -18), S( -26,   28), S(  -9,   19), S( -10,   47), S(  -2,   31), S(  -4,   34), S(   3,   12), S(  -3,   11),
    S( -14,  -16), S(   2,  -27), S( -11,   15), S(  -2,    6), S(  -5,    9), S(   2,   17), S(  14,   10), S(   5,    5),
    S( -35,  -22), S(  -8,  -23), S(  11,  -30), S(   2,  -16), S(   8,  -16), S(  15,  -23), S(  -3,  -36), S(   1,  -32),
    S(  -1,  -33), S( -18,  -28), S(  -9,  -22), S(  10,  -43), S( -15,   -5), S( -25,  -32), S( -31,  -20), S( -50,  -41)
  };

  Score king_table[64] = {
    S( -65,  -74), S(  23,  -35), S(  16,  -18), S( -15,  -18), S( -56,  -11), S( -34,   15), S(   2,    4), S(  13,  -17),
    S(  29,  -12), S(  -1,   17), S( -20,   14), S(  -7,   17), S(  -8,   17), S(  -4,   38), S( -38,   23), S( -29,   11),
    S(  -9,   10), S(  24,   17), S(   2,   23), S( -16,   15), S( -20,   20), S(   6,   45), S(  22,   44), S( -22,   13),
    S( -17,   -8), S( -20,   22), S( -12,   24), S( -27,   27), S( -30,   26), S( -25,   33), S( -14,   26), S( -36,    3),
    S( -49,  -18), S(  -1,   -4), S( -27,   21), S( -39,   24), S( -46,   27), S( -44,   23), S( -33,    9), S( -51,  -11),
    S( -14,  -19), S( -14,   -3), S( -22,   11), S( -46,   21), S( -44,   23), S( -30,   16), S( -15,    7), S( -27,   -9),
    S(   1,  -27), S(   7,  -11), S(  -8,    4), S( -64,   13), S( -43,   14), S( -16,    4), S(   9,   -5), S(   8,  -17),
    S( -15,  -53), S(  36,  -34), S(  12,  -21), S( -54,  -11), S(   8,  -28), S( -28,  -14), S(  24,  -24), S(  14,  -43)
  };

  // TODO: Add const here
  // Score pawn_table[64] = {
  //   S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), 
  //   S( -30,   33), S( -25,   40), S( -29,   49), S( -26,    0), S(  -9,   28), S(  29,   40), S(  49,   38), S(  18,   -1), 
  //   S( -22,    3), S( -14,   20), S(  -9,   12), S(  -5,   -2), S(  19,   12), S(  31,    6), S(  51,   20), S(  29,  -11), 
  //   S( -14,   28), S( -11,   27), S(   7,    6), S(  17,   18), S(  23,   -5), S(  36,   20), S(  41,   26), S(  29,    9), 
  //   S(   9,   63), S(   7,   42), S(  18,   32), S(  37,   17), S(  52,   15), S(  50,   36), S(  71,   55), S(  31,   37), 
  //   S( -11,   47), S(   7,   52), S(  11,   42), S(  38,   13), S(  80,   12), S( 110,   17), S(  72,   75), S(  16,   38), 
  //   S(  44,  124), S(  80,  119), S(   7,  104), S(  41,   80), S(  14,   93), S(  72,   78), S(  24,  111), S( -42,  133), 
  //   S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0), S(   0,    0)
  // };

  // Score knight_table[64] = {
  //   S( -95,  -83), S( -47,  -53), S( -72,  -33), S( -39,  -28), S( -27,  -32), S( -32,  -38), S( -32,  -97), S( -66,  -75), 
  //   S( -53,  -45), S( -74,  -52), S( -20,   11), S( -14,  -15), S(  -6,  -12), S(   8,  -18), S( -24,  -31), S( -19,  -26), 
  //   S( -44,  -32), S( -15,  -13), S( -14,   15), S(   0,   25), S(  26,   22), S(   7,  -13), S(  19,  -20), S( -11,  -43), 
  //   S( -17,  -16), S(  -6,   15), S(  18,   18), S(   3,   38), S(  18,   25), S(  37,   37), S(  38,   14), S(  18,  -14), 
  //   S(   1,  -13), S(  19,   11), S(   9,   10), S(  42,   66), S(  15,   65), S(  70,   60), S(  57,   53), S(  72,   -7), 
  //   S( -37,  -14), S(  22,  -10), S( -17,   16), S(  75,   20), S(  94,   19), S( 139,    1), S(  83,  -10), S(  54,  -51), 
  //   S( -39,  -16), S( -49,   -8), S(  82,  -15), S( -18,    7), S(  52,    1), S(  72,  -15), S(  17,  -11), S(  11,  -20), 
  //   S(-177, -112), S( -99,  -20), S( -80,   -6), S( -59,  -18), S(  71,  -21), S(-107,  -21), S( -25,  -74), S(-117, -141)
  // };

  // Score bishop_table[64] = {
  //   S( -23,  -13), S(   7,  -14), S( -27,  -33), S( -52,  -24), S( -44,  -19), S( -36,  -48), S( -37,  -15), S( -75,  -27), 
  //   S(   0,  -35), S(   0,  -29), S(  20,    3), S( -21,   -4), S( -15,   -6), S(  11,  -11), S(  18,  -18), S(  -9,  -31), 
  //   S(   7,  -29), S(   5,    1), S( -15,   19), S(   1,   15), S(   4,   -1), S( -12,    5), S(   8,  -11), S(  20,  -12), 
  //   S( -17,  -10), S( -17,   -4), S(  -9,   26), S(  13,   26), S(   4,   16), S(   2,    5), S( -23,  -13), S(   1,  -35), 
  //   S(   3,   -8), S(  -5,   25), S(   1,   20), S(  25,   55), S(  13,   29), S(  13,    6), S(  -2,   13), S( -24,    6), 
  //   S( -22,    8), S(  27,  -26), S( -11,    9), S(   3,   21), S(  28,    6), S(  18,   16), S(  47,    8), S(  38,   17), 
  //   S( -27,  -16), S(   9,   -5), S( -19,  -10), S( -60,   30), S( -24,   13), S(  22,   11), S(   8,  -18), S( -37,    0), 
  //   S( -39,  -23), S(  -6,  -21), S( -92,   35), S( -91,  -59), S( -26,    3), S( -52,  -50), S(  17,   -6), S( -62,  -35)
  // };

  // Score rook_table[64] = {
  //   S( -29,  -17), S( -12,    0), S(  -9,    2), S(  13,  -11), S(  14,  -11), S(   5,  -14), S(  17,   14), S( -26,  -16), 
  //   S( -54,  -35), S( -26,  -31), S( -30,  -11), S( -19,   -8), S( -11,  -21), S(   1,  -30), S( -11,  -23), S( -41,  -15), 
  //   S( -22,  -13), S( -13,   -6), S( -30,   -8), S( -26,  -10), S(   7,   -4), S(  22,  -15), S(  49,   -7), S(  21,   -8), 
  //   S( -22,   14), S( -23,    7), S( -22,   15), S( -11,   -2), S(  -1,   -9), S(  16,   -9), S(  51,  -10), S(  31,    4), 
  //   S( -29,   31), S(  -1,   30), S( -20,   31), S(  28,    5), S(  17,    6), S(  69,   -7), S(  46,   10), S(  34,    9), 
  //   S(  27,   17), S(  10,    3), S(   7,   24), S(  46,   10), S(  61,    9), S(  98,    3), S( 115,   -7), S(  70,    9), 
  //   S(  -6,  -10), S(  16,  -30), S(  28,   -7), S(  33,   -6), S(  67,  -27), S(  77,  -22), S(  36,    1), S(  54,  -10), 
  //   S(  42,   17), S(  78,   31), S(  42,   32), S(  61,   25), S(  73,   22), S(  63,   40), S(  57,   27), S(  53,   18)
  // };

  // Score queen_table[64] = {
  //   S( -14,  -87), S( -28,  -39), S( -19,  -34), S(   0,  -53), S( -17,  -15), S( -35,  -42), S( -41,  -30), S( -54,  -45), 
  //   S( -65,  -75), S( -18,  -74), S(  -3,  -40), S(   4,  -26), S(   8,  -26), S(   5,  -33), S(  -4,  -46), S(  20,  -42), 
  //   S( -24,  -52), S(   5,  -40), S( -10,    0), S( -11,   -4), S(  -2,    0), S(   6,   14), S(  27,    3), S(  32,    7), 
  //   S(  -1,  -48), S( -31,   -4), S( -12,   17), S(  -6,   52), S(  -3,   41), S(   7,   45), S(  31,   30), S(  38,   49), 
  //   S( -34,  -39), S(  -7,   -1), S( -23,   29), S(  16,   57), S(  15,   79), S(  40,   92), S(  52,  105), S(  55,   90), 
  //   S( -23,  -48), S( -20,   12), S(  33,   34), S(  30,   59), S(  39,   57), S(  66,   56), S(  64,   29), S(  81,   19), 
  //   S( -26,  -42), S( -21,    7), S(   1,   15), S(  10,   49), S(  17,   68), S(  67,   35), S(  38,   40), S(  64,   10), 
  //   S( -38,  -57), S(  10,   -4), S(  39,    6), S(  22,   37), S(  69,   37), S(  54,   29), S(  53,   20), S(  55,   16)
  // };

  // Score king_table[64] = {
  //   S(  19,  -43), S(  89,    0), S(  66,   -3), S( -40,  -21), S(  33,  -38), S( -38,   -5), S(  59,   -6), S(  32,  -84), 
  //   S(   9,    3), S(  41,   -8), S(   5,   10), S( -68,    3), S( -53,    9), S( -26,    5), S(  30,    1), S(  20,  -12), 
  //   S( -28,  -20), S(  -4,    5), S( -32,    1), S( -62,   11), S( -54,   16), S( -40,    6), S( -25,   -3), S( -81,  -10), 
  //   S(-103,  -30), S( -22,   15), S( -46,   20), S( -51,   45), S(-100,   27), S( -54,   22), S( -87,   12), S(-105,  -24), 
  //   S( -71,  -18), S( -45,   32), S( -47,   60), S( -69,   58), S( -68,   60), S( -33,   49), S( -68,   46), S( -90,  -21), 
  //   S( -40,    0), S(  34,   29), S(  12,   33), S( -18,   38), S( -10,   39), S(   5,   58), S(  27,   49), S( -76,   10), 
  //   S( -23,   -9), S(  44,   27), S( -10,   24), S(   3,   27), S( -17,   49), S(   6,   48), S( -30,   48), S( -26,    6), 
  //   S( -74,  -84), S(  27,  -17), S(  34,    3), S( -21,   -8), S( -18,    3), S( -32,   24), S(  -8,  -11), S( -39,  -71)
  // };

  // const int *mg_pesto_table[6] = {mg_pawn_table, mg_knight_table, mg_bishop_table, mg_rook_table, mg_queen_table, mg_king_table};
  // const int *eg_pesto_table[6] = {eg_pawn_table, eg_knight_table, eg_bishop_table, eg_rook_table, eg_queen_table, eg_king_table};

  /// Main access array for PeSTO Tables
  const Score *pesto_table[6] = {pawn_table, knight_table, bishop_table, rook_table, queen_table, king_table};

  inline void evaluate_pawns(U64 pawns, U64 enemy_pawns, int color, Score &score) {
    U64 p_copy = pawns;
    while (p_copy) {
      int sq = Bitboard::pop_lsb(p_copy);
      int file = sq % 8;

      // Penalize isolated pawns
      if (!(pawns & isolated_pawn_masks[file])) {
        score -= S(15, 15);
      }

      // Reward passed pawns
      if (!(enemy_pawns & passed_pawn_masks[color][sq])) {
        int rank = (color == WHITE) ? (sq / 8) : 7 - (sq / 8);
        score += passed_pawn_bonus[rank];
      }

      // Penalize doubled pawns
      if (pawns & FILE_MASKS[file] & ~(1ULL << sq)) {
        score -= S(15, 20);
      }
    }
  }

  inline void evaluate_king_safety(int king_sq, U64 friendly_pawns, U64 enemy_pawns, Score &score, bool enemy_has_queen) {
    if (!enemy_has_queen) return;

    int file = king_sq % 8;
    if (file >= 5 || file <= 2) {
      int shield_penalties = 0;
      for (int f = std::max(0, file - 1); f <= std::min(7, file + 1); f++) {
        U64 file_mask = FILE_MASKS[f];
        if (!(friendly_pawns & file_mask)) {
          shield_penalties += 25;
        }
        if (!(enemy_pawns & file_mask) && !(friendly_pawns & file_mask)) {
          shield_penalties += 15;
        }
      }
      score.mg -= shield_penalties;
    }
  }

  /// evaluate() is the core classical evaluation function. It assesses material, 
  /// piece mobility, pawn structure, king safety, and positional bonuses.

  int evaluate(const Position &pos)
  {
    if (pos.piece_bb[WHITE][KING] == 0 || pos.piece_bb[BLACK][KING] == 0)
      return 0;
    
    Score score[2] = { S(0, 0), S(0, 0) };
    int phase = 0;

    const U64 all_occ = pos.occ[BOTH];
    score[pos.side_to_move].mg += TEMPO_BONUS;

    bool white_has_queen = pos.piece_bb[WHITE][QUEEN] != 0;
    bool black_has_queen = pos.piece_bb[BLACK][QUEEN] != 0;

    if (Bitboard::popcount(pos.piece_bb[WHITE][BISHOP]) >= 2) {
      score[WHITE] += BISHOP_PAIR;
    }
    if (Bitboard::popcount(pos.piece_bb[BLACK][BISHOP]) >= 2) {
      score[BLACK] += BISHOP_PAIR;
    }

    int w_king_sq = Bitboard::lsb(pos.piece_bb[WHITE][KING]);
    int b_king_sq = Bitboard::lsb(pos.piece_bb[BLACK][KING]);
    U64 w_king_ring = Bitboard::king_attacks[w_king_sq];
    U64 b_king_ring = Bitboard::king_attacks[b_king_sq];

    for (int color = 0; color <= 1; color++) {
      int enemy = color ^ 1;
      U64 enemy_king_ring = (color == WHITE) ? b_king_ring : w_king_ring;
      U64 enemy_queen = pos.piece_bb[enemy][QUEEN];

      for (int p = PAWN; p <= KING; p++) {
        U64 pieces = pos.piece_bb[color][p];
        phase += Bitboard::popcount(pieces) * phase_weights[p];

        while (pieces) {
          int sq = Bitboard::pop_lsb(pieces);
          score[color] += piece_value[p];

          int pst_sq = (color == WHITE) ? (sq ^ 56) : sq;
          score[color] += pesto_table[p][pst_sq];

          int rank = (color == WHITE) ? (sq / 8) : 7 - (sq / 8);

          if (p == KNIGHT) {
            U64 attacks = Bitboard::knight_attacks[sq];
            int mobility = Bitboard::popcount(attacks & ~pos.occ[color]);
            score[color] += S((mobility - 4) * 4, (mobility - 4) * 4);
            if (attacks & enemy_king_ring) score[color].mg += 10;
            if (attacks & enemy_queen) score[color] += THREAT_ON_QUEEN;

            // 1. Minor Behind Pawn
            int pawn_behind_sq = (color == WHITE) ? sq + 8 : sq - 8;
            if (pawn_behind_sq >= 0 && pawn_behind_sq < 64 && (pos.piece_bb[color][PAWN] & (1ULL << pawn_behind_sq))) {
              score[color] += MINOR_BEHIND_PAWN;
            }

            // 2. Outpost
            // if (rank >= 3 && rank <= 5) {
            //   if (!(enemy_pawn_attack_span(enemy, sq) & pos.piece_bb[enemy][PAWN])) {
            //     score[color] += OUTPOST_BONUS;
            //   }
            // }
          } 
          else if (p == BISHOP) {
            U64 attacks = Bitboard::get_bishop_attacks(sq, all_occ);
            int mobility = Bitboard::popcount(attacks & ~pos.occ[color]);
            score[color] += S((mobility - 7) * 3, (mobility - 7) * 3);
            if (attacks & enemy_king_ring) score[color].mg += 10;
            if (attacks & enemy_queen) score[color] += THREAT_ON_QUEEN;

            // Minor Behind Pawn
            int pawn_behind_sq = (color == WHITE) ? sq + 8 : sq - 8;
            if (pawn_behind_sq >= 0 && pawn_behind_sq < 64 && (pos.piece_bb[color][PAWN] & (1ULL << pawn_behind_sq))) {
              score[color] += MINOR_BEHIND_PAWN;
            }
          } 
          else if (p == ROOK) {
            U64 attacks = Bitboard::get_rook_attacks(sq, all_occ);
            int mobility = Bitboard::popcount(attacks & ~pos.occ[color]);
            score[color] += S((mobility - 7) * 2, (mobility - 7) * 3);
            if (attacks & enemy_king_ring) score[color].mg += 15;
            if (attacks & enemy_queen) score[color] += THREAT_ON_QUEEN;

            // 3. Trapped Rook Penalty
            if (mobility <= 3) {
              int k_sq = (color == WHITE) ? w_king_sq : b_king_sq;
              int k_file = k_sq % 8;
              int r_file = sq % 8;
              if ((k_file < 4 && r_file < k_file) || (k_file > 3 && r_file > k_file)) {
                score[color] -= TRAPPED_ROOK_PENALTY;
              }
            }

            U64 file_mask = FILE_MASKS[sq % 8];
            if (!(pos.piece_bb[color][PAWN] & file_mask)) {
              if (!(pos.piece_bb[enemy][PAWN] & file_mask)) {
                score[color] += ROOK_OPEN;
              } else {
                score[color] += ROOK_SEMI_OPEN;
              }
            }

            if (rank == 6) {
              score[color] += S(30, 30); // Huge bonus
            }
          }
          else if (p == QUEEN) {
            U64 attacks = Bitboard::get_queen_attacks(sq, all_occ);
            if (attacks & enemy_king_ring) score[color].mg += 15;
          }
        }
      }

      evaluate_pawns(pos.piece_bb[color][PAWN], pos.piece_bb[enemy][PAWN], color, score[color]);

      int k_sq = (color == WHITE) ? w_king_sq : b_king_sq;
      bool enemy_has_queen = (color == WHITE) ? black_has_queen : white_has_queen;
      evaluate_king_safety(k_sq, pos.piece_bb[color][PAWN], pos.piece_bb[enemy][PAWN], score[color], enemy_has_queen);
    }

    if (phase > 24)
      phase = 24;

    int mg_total = score[WHITE].mg - score[BLACK].mg;
    int eg_total = score[WHITE].eg - score[BLACK].eg;

    if (phase <= 12) {
      int mop_up_score = 0;
      if (eg_total > 300) { 
        mop_up_score += center_distance[b_king_sq] * 10;
        int dist = std::abs((w_king_sq / 8) - (b_king_sq / 8)) + std::abs((w_king_sq % 8) - (b_king_sq % 8));
        mop_up_score += (14 - dist) * 4;
        eg_total += mop_up_score;
      } else if (eg_total < -300) {
        mop_up_score += center_distance[w_king_sq] * 10;
        int dist = std::abs((b_king_sq / 8) - (w_king_sq / 8)) + std::abs((b_king_sq % 8) - (w_king_sq % 8));
        mop_up_score += (14 - dist) * 4;
        eg_total -= mop_up_score;
      }
    }

    int evaluation = (mg_total * phase + eg_total * (MAX_PHASE - phase)) / MAX_PHASE;
    int classical_score = (pos.side_to_move == WHITE) ? evaluation : -evaluation;

    return classical_score;
  }
}
