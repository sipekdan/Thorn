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

#include <iostream>
#include <cstdint>
#include <string>
#include <cassert>

using U64 = uint64_t;
using Move = uint16_t;

enum Square : int {
  SQ_A1, SQ_B1, SQ_C1, SQ_D1, SQ_E1, SQ_F1, SQ_G1, SQ_H1,
  SQ_A2, SQ_B2, SQ_C2, SQ_D2, SQ_E2, SQ_F2, SQ_G2, SQ_H2,
  SQ_A3, SQ_B3, SQ_C3, SQ_D3, SQ_E3, SQ_F3, SQ_G3, SQ_H3,
  SQ_A4, SQ_B4, SQ_C4, SQ_D4, SQ_E4, SQ_F4, SQ_G4, SQ_H4,
  SQ_A5, SQ_B5, SQ_C5, SQ_D5, SQ_E5, SQ_F5, SQ_G5, SQ_H5,
  SQ_A6, SQ_B6, SQ_C6, SQ_D6, SQ_E6, SQ_F6, SQ_G6, SQ_H6,
  SQ_A7, SQ_B7, SQ_C7, SQ_D7, SQ_E7, SQ_F7, SQ_G7, SQ_H7,
  SQ_A8, SQ_B8, SQ_C8, SQ_D8, SQ_E8, SQ_F8, SQ_G8, SQ_H8,
  SQ_NONE = 64
};

inline bool is_orthogonal(int sq1, int sq2) { return (sq1 / 8 == sq2 / 8) || (sq1 % 8 == sq2 % 8); }
inline bool is_diagonal(int sq1, int sq2) { return abs((sq1 / 8) - (sq2 / 8)) == abs((sq1 % 8) - (sq2 % 8)); }

enum Color : uint8_t {
  WHITE,
  BLACK,
  BOTH
};

enum Piece : uint8_t {
  PAWN,
  KNIGHT,
  BISHOP,
  ROOK,
  QUEEN,
  KING,
  NONE
};

constexpr int INFINITY_SCORE = 50000;
constexpr int MATE_SCORE = 49000;

constexpr uint8_t WK_CASTLE = 1, WQ_CASTLE = 2, BK_CASTLE = 4, BQ_CASTLE = 8;

inline int move_from(Move m) { return m & 0x3F; }
inline int move_to(Move m) { return (m >> 6) & 0x3F; }
inline Piece move_prom(Move m) { return (Piece)(m >> 12); }

inline Move make_move_16(int from, int to, int prom = NONE) {
  return from | (to << 6) | (prom << 12);
}

struct MoveList {
  Move moves[256];
  int count = 0;
  inline void push(int from, int to, int prom = NONE) {
    moves[count++] = make_move_16(from, to, prom);
  }
};

struct UndoInfo {
  U64 hash_key;
  int8_t ep_square;
  uint8_t castling_rights;
  Piece captured_piece;
  bool is_ep;
  int halfmove_clock;
};

inline std::string move_to_string(Move m) {
  if (m == 0) return "(none)";

  std::string s = "";
  int from = move_from(m);
  int to = move_to(m);
  Piece prom = move_prom(m);
  s += (char)('a' + (from % 8));
  s += (char)('1' + (from / 8));
  s += (char)('a' + (to % 8));
  s += (char)('1' + (to / 8));
  if (prom == QUEEN) s += "q";
  if (prom == ROOK) s += "r";
  if (prom == BISHOP) s += "b";
  if (prom == KNIGHT) s += "n";
  return s;
}

