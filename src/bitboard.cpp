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

#include <iomanip>

#include "bitboard.h"
#include <algorithm>

namespace Bitboard {

  /// knight_attacks[sq] contains a bitboard of all squares a knight can attack from 'sq'.
  U64 knight_attacks[64];

  /// king_attacks[sq] contains a bitboard of all squares a king can step to from 'sq'.
  U64 king_attacks[64];

  /// pawn_attacks[color][sq] contains the diagonal capture squares for a pawn of 'color' at 'sq'.
  U64 pawn_attacks[2][64];

  /// between_bb[sq1][sq2] contains a bitboard of the squares strictly between sq1 and sq2.
  /// If the squares are not on the same rank, file, or diagonal, it returns 0.
  U64 between_bb[64][64];

  /// castling_board[sq] is used to update castling rights efficiently during move making.
  /// It holds a bitmask for every square. When a piece moves from or to a square, the 
  /// global castling rights are bitwise ANDed with this array. Normal squares hold 15 
  /// (binary 1111, meaning no rights are lost). Rook and King starting squares hold 
  /// masks that clear their respective castling rights (e.g., clearing the bit for WK_CASTLE).
  uint8_t castling_board[64];

  /// Magic bitboards arrays for sliding pieces
  U64 rook_masks[64], bishop_masks[64];
  U64 rook_magics[64], bishop_magics[64];
  int rook_shifts[64], bishop_shifts[64];
  int rook_offsets[64], bishop_offsets[64];

  /// attacks[]: The final lookup tables storing every possible attack configuration.
  U64 rook_attacks[102400];
  U64 bishop_attacks[5248];

  /// classical_bishop_attacks() calculates bishop attacks for a given square and
  /// occupancy using slow, loop-based ray casting. Used only during initialization.

  U64 classical_bishop_attacks(int sq, U64 occ) {
    U64 attacks = 0;
    int tr = sq / 8, tf = sq % 8;

    // Cast rays in all 4 diagonal directions until an occupied square is hit
    for (int r = tr + 1, f = tf + 1; r <= 7 && f <= 7; r++, f++) { attacks |= (1ULL << (r * 8 + f)); if (occ & (1ULL << (r * 8 + f))) break; }
    for (int r = tr + 1, f = tf - 1; r <= 7 && f >= 0; r++, f--) { attacks |= (1ULL << (r * 8 + f)); if (occ & (1ULL << (r * 8 + f))) break; }
    for (int r = tr - 1, f = tf + 1; r >= 0 && f <= 7; r--, f++) { attacks |= (1ULL << (r * 8 + f)); if (occ & (1ULL << (r * 8 + f))) break; }
    for (int r = tr - 1, f = tf - 1; r >= 0 && f >= 0; r--, f--) { attacks |= (1ULL << (r * 8 + f)); if (occ & (1ULL << (r * 8 + f))) break; }

    return attacks;
  }

  /// classical_rook_attacks() calculates rook attacks for a given square and
  /// occupancy using slow, loop-based ray casting. Used only during initialization.

  U64 classical_rook_attacks(int sq, U64 occ) {
    U64 attacks = 0;
    int tr = sq / 8, tf = sq % 8;

    // Cast rays in all 4 orthogonal directions until an occupied square is hit
    for (int r = tr + 1; r <= 7; r++) { attacks |= (1ULL << (r * 8 + tf)); if (occ & (1ULL << (r * 8 + tf))) break; }
    for (int r = tr - 1; r >= 0; r--) { attacks |= (1ULL << (r * 8 + tf)); if (occ & (1ULL << (r * 8 + tf))) break; }
    for (int f = tf + 1; f <= 7; f++) { attacks |= (1ULL << (tr * 8 + f)); if (occ & (1ULL << (tr * 8 + f))) break; }
    for (int f = tf - 1; f >= 0; f--) { attacks |= (1ULL << (tr * 8 + f)); if (occ & (1ULL << (tr * 8 + f))) break; }

    return attacks;
  }

  /// set_occupancy() maps an index to a specific subset of bits in the attack_mask.
  /// This generates every possible blocker configuration for magic bitboards.

  U64 set_occupancy(int index, int bits_in_mask, U64 attack_mask) {
    U64 occupancy = 0;
    for (int i = 0; i < bits_in_mask; i++) {
      int sq = lsb(attack_mask); // Get the least significant bit (first square in mask)
      attack_mask &= attack_mask - 1; // Clear that bit

      // If the i-th bit of our index is 1, place a blocker on that square
      if (index & (1 << i)) occupancy |= (1ULL << sq);
    }
    return occupancy;
  }

  /// init_leapers() precalculates attack bitboards for non-sliding pieces
  /// (Pawns, Knights, Kings) and sets up the castling rights mask array.

  void init_leapers() {
    for (int sq = 0; sq < 64; sq++) {
      int r = sq / 8, f = sq % 8;
      pawn_attacks[WHITE][sq] = pawn_attacks[BLACK][sq] = knight_attacks[sq] = king_attacks[sq] = 0;

      // Pawn captures (diagonal forward steps)
      if (r < 7 && f > 0) pawn_attacks[WHITE][sq] |= (1ULL << (sq + 7));
      if (r < 7 && f < 7) pawn_attacks[WHITE][sq] |= (1ULL << (sq + 9));
      if (r > 0 && f > 0) pawn_attacks[BLACK][sq] |= (1ULL << (sq - 9));
      if (r > 0 && f < 7) pawn_attacks[BLACK][sq] |= (1ULL << (sq - 7));

      // Knight jumps (L-shapes)
      int kn[8][2] = {{-2,-1},{-2,1},{-1,-2},{-1,2},{1,-2},{1,2},{2,-1},{2,1}};
      for (int i = 0; i < 8; i++) {
        int nr = r + kn[i][0], nf = f + kn[i][1];
        if (nr >= 0 && nr <= 7 && nf >= 0 && nf <= 7) knight_attacks[sq] |= (1ULL << (nr * 8 + nf));
      }

      // King moves (1 step in any direction)
      int km[8][2] = {{-1,-1},{-1,0},{-1,1},{0,-1},{0,1},{1,-1},{1,0},{1,1}};
      for (int i = 0; i < 8; i++) {
        int nr = r + km[i][0], nf = f + km[i][1];
        if (nr >= 0 && nr <= 7 && nf >= 0 && nf <= 7) king_attacks[sq] |= (1ULL << (nr * 8 + nf));
      }
      
      // By default, moving to or from any square doesn't affect castling rights
      castling_board[sq] = 15;
    }

    // Set specific masks to clear castling rights when king or rooks move/are captured.
    castling_board[0] = 13;  // White Queenside Rook
    castling_board[4] = 12;  // White King
    castling_board[7] = 14;  // White Kingside Rook
    castling_board[56] = 7;  // Black Queenside Rook
    castling_board[60] = 3;  // Black King
    castling_board[63] = 11; // Black Kingside Rook
  }

  /// init_magics() finds magic numbers and populates the sliding attack tables
  /// for rooks and bishops using a pseudo-random number generator.

  void init_magics() {
    U64 seed = 1070372;
    auto random_u64 = [&seed]() {
      seed ^= seed >> 12; seed ^= seed << 25; seed ^= seed >> 27;
      return seed * 2685821657736338717ULL;
    };
    auto random_fewbits = [&]() { return random_u64() & random_u64() & random_u64(); };

    int b_offset = 0, r_offset = 0;
    U64 b_blockers[4096], b_attacks_calc[4096], used_b[4096];
    U64 r_blockers[4096], r_attacks_calc[4096], used_r[4096];

    for (int sq = 0; sq < 64; sq++) {
      // --- BISHOP MAGICS ---
      bishop_masks[sq] = 0;
      int r = sq / 8, f = sq % 8;
      for (int i=r+1, j=f+1; i<=6 && j<=6; i++, j++) bishop_masks[sq] |= (1ULL << (i * 8 + j));
      for (int i=r+1, j=f-1; i<=6 && j>=1; i++, j--) bishop_masks[sq] |= (1ULL << (i * 8 + j));
      for (int i=r-1, j=f+1; i>=1 && j<=6; i--, j++) bishop_masks[sq] |= (1ULL << (i * 8 + j));
      for (int i=r-1, j=f-1; i>=1 && j>=1; i--, j--) bishop_masks[sq] |= (1ULL << (i * 8 + j));

      int b_bits = popcount(bishop_masks[sq]);
      int b_combos = 1 << b_bits;
      bishop_shifts[sq] = 64 - b_bits;
      bishop_offsets[sq] = b_offset;

      for (int i = 0; i < b_combos; i++) {
        b_blockers[i] = set_occupancy(i, b_bits, bishop_masks[sq]);
        b_attacks_calc[i] = classical_bishop_attacks(sq, b_blockers[i]);
      }

      for (int i = 0; i < 4096; i++) used_b[i] = 0;
      while (true) {
        U64 magic = random_fewbits();
        if (popcount((bishop_masks[sq] * magic) & 0xFF00000000000000ULL) < 6) continue;
        bool found = true;
        for (int i = 0; i < 4096; i++) used_b[i] = 0;
        for (int i = 0; i < b_combos; i++) {
          int idx = (b_blockers[i] * magic) >> bishop_shifts[sq];
          if (used_b[idx] == 0 || used_b[idx] == b_attacks_calc[i]) used_b[idx] = b_attacks_calc[i];
          else { found = false; break; }
        }
        if (found) {
          bishop_magics[sq] = magic;
          for (int i = 0; i < b_combos; i++) {
            int idx = (b_blockers[i] * magic) >> bishop_shifts[sq];
            bishop_attacks[b_offset + idx] = b_attacks_calc[i];
          }
          b_offset += b_combos; break;
        }
      }

      // --- ROOK MAGICS ---
      rook_masks[sq] = 0;
      for (int i = r + 1; i <= 6; i++) rook_masks[sq] |= (1ULL << (i * 8 + f));
      for (int i = r - 1; i >= 1; i--) rook_masks[sq] |= (1ULL << (i * 8 + f));
      for (int i = f + 1; i <= 6; i++) rook_masks[sq] |= (1ULL << (r * 8 + i));
      for (int i = f - 1; i >= 1; i--) rook_masks[sq] |= (1ULL << (r * 8 + i));

      int r_bits = popcount(rook_masks[sq]);
      int r_combos = 1 << r_bits;
      rook_shifts[sq] = 64 - r_bits;
      rook_offsets[sq] = r_offset;

      for (int i = 0; i < r_combos; i++) {
        r_blockers[i] = set_occupancy(i, r_bits, rook_masks[sq]);
        r_attacks_calc[i] = classical_rook_attacks(sq, r_blockers[i]);
      }

      for (int i = 0; i < 4096; i++) used_r[i] = 0;

      while (true) {
        U64 magic = random_fewbits();
        if (popcount((rook_masks[sq] * magic) & 0xFF00000000000000ULL) < 6) continue;
        bool found = true;
        for (int i = 0; i < 4096; i++) used_r[i] = 0;
        for (int i = 0; i < r_combos; i++) {
          int idx = (r_blockers[i] * magic) >> rook_shifts[sq];
          if (used_r[idx] == 0 || used_r[idx] == r_attacks_calc[i]) used_r[idx] = r_attacks_calc[i];
          else { found = false; break; }
        }
        if (found) {
          rook_magics[sq] = magic;
          for (int i = 0; i < r_combos; i++) {
              int idx = (r_blockers[i] * magic) >> rook_shifts[sq];
              rook_attacks[r_offset + idx] = r_attacks_calc[i];
          }
          r_offset += r_combos; break;
        }
      }
    }
  }

  /// init_between() precalculates the bitboard of squares strictly between 
  /// any two squares on the board. Useful for sliding check/pin validation.

  void init_between() {
    for (int i = 0; i < 64; ++i) {
      for (int j = 0; j < 64; ++j) {
        between_bb[i][j] = 0;
        if (i == j) continue;

        if (get_bishop_attacks(i, 0) & (1ULL << j)) {
          between_bb[i][j] = get_bishop_attacks(i, 1ULL << j) & get_bishop_attacks(j, 1ULL << i);
        }
        else if (get_rook_attacks(i, 0) & (1ULL << j)) {
          between_bb[i][j] = get_rook_attacks(i, 1ULL << j) & get_rook_attacks(j, 1ULL << i);
        }
      }
    }
  }

  /// init() is the primary entry point to prepare all bitboards at engine startup.

  void init() {
    init_leapers();
    init_magics();
    init_between();
  }

  /// print() outputs a visualization of a 64-bit integer bitboard to the console, 
  /// useful for debugging masks and attack arrays.

  void print(U64 bb) {
    std::cout << "\n";
    for (int r = 7; r >= 0; r--) {
      std::cout << "  " << r + 1 << " ";
      for (int f = 0; f < 8; f++) {
        int sq = r * 8 + f;
        if (bb & (1ULL << sq)) {
          std::cout << " X ";
        } else {
          std::cout << " . ";
        }
      }
      std::cout << "\n";
    }

    std::cout << "\n     a  b  c  d  e  f  g  h\n";
    std::cout << "\n     Hex: 0x" << std::hex << std::uppercase << std::setfill('0') << std::setw(16) << bb << std::dec;
    std::cout << "\n     Dec: " << bb << "ULL\n\n";
  }
}
