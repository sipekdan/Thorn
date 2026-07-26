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

namespace Bitboard {
  extern U64 knight_attacks[64];
  extern U64 king_attacks[64];
  extern U64 pawn_attacks[2][64];    // [Color][Square]
  extern U64 between_bb[64][64];     // The mask of squares between two squares
  extern uint8_t castling_board[64]; // Masks out castling rights if a King or Rook moves

  extern U64 rook_masks[64], bishop_masks[64];
  extern U64 rook_magics[64], bishop_magics[64];
  extern int rook_shifts[64], bishop_shifts[64];
  extern int rook_offsets[64], bishop_offsets[64];

  /// The giant lookup tables storing every possible attack configuration.
  extern U64 rook_attacks[102400];
  extern U64 bishop_attacks[5248];

  // inline void set_bit(U64 &b, int sq) { b |= (1ULL << sq); }
  // inline void clear_bit(U64 &b, int sq) { b &= ~(1ULL << sq); }
  // inline U64 square_bb(int sq) { return 1ULL << sq; }
  
  /// init() computes and populates all the bitboard arrays (leapers, magics, between) at startup.
  void init();

  /// print() outputs a visualization of a 64-bit integer bitboard to the console.
  void print(U64 bb);

  /// get_rook_attacks() returns the attack bitboard for a rook on 'sq' given the current 'occ' (occupancy).
  /// It masks the occupancy, multiplies by the magic number, and shifts down to get a unique array index.
  inline U64 get_rook_attacks(int sq, U64 occ) {
    occ &= rook_masks[sq];
    return rook_attacks[rook_offsets[sq] + ((occ * rook_magics[sq]) >> rook_shifts[sq])];
  }

  /// get_bishop_attacks() returns the attack bitboard for a bishop on 'sq' given the current 'occ'.
  inline U64 get_bishop_attacks(int sq, U64 occ) {
    occ &= bishop_masks[sq];
    return bishop_attacks[bishop_offsets[sq] + ((occ * bishop_magics[sq]) >> bishop_shifts[sq])];
  }

  /// get_queen_attacks() combines rook and bishop attacks, as a queen moves as both.
  inline U64 get_queen_attacks(int sq, U64 occ) {
    return get_rook_attacks(sq, occ) | get_bishop_attacks(sq, occ);
  }

  /// popcount() counts the number of set bits (1s) in a 64-bit integer.
  /// Uses a blazing fast compiler intrinsic mapped to a dedicated CPU instruction (e.g., POPCNT).
  inline int popcount(U64 bb) {
    return __builtin_popcountll(bb);
  }

  /// lsb() finds the index (0-63) of the Least Significant Bit in a 64-bit integer.
  /// Uses a compiler intrinsic (Count Trailing Zeros) mapped to a CPU instruction (e.g., BSF/TZCNT).
  inline int lsb(U64 bb) {
    return __builtin_ctzll(bb);
  }

  /// set_bit() sets the bit at the given square 'sq' to 1.
  inline void set_bit(U64& bb, int sq) {
    bb |= (1ULL << sq);
  }

  /// pop_lsb() finds the index of the Least Significant Bit, clears it from the bitboard, 
  /// and returns the index. Used heavily in move generation to loop over pieces.
  inline int pop_lsb(U64& bb) {
    int sq = lsb(bb);
    bb &= bb - 1;
    return sq;
  }
}