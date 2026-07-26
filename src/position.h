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

#include <string>

#include "types.h"
#include "bitboard.h"
#include "zobrist.h"

const std::string START_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

struct Position
{
  alignas(64) U64 piece_bb[2][6] = {0}; // [Color][Piece]
  alignas(64) U64 occ[3] = {0};         // [White, Black, Both]
  alignas(64) Piece board_sq[64];       // Mailbox board for fast lookups

  U64 hash_key = 0;
  int side_to_move = WHITE;
  uint8_t castling_rights = 0;
  int8_t ep_square = -1;

  int halfmove_clock = 0;
  int fullmove_number = 1;

  U64 repetition_table[2048] = {0};
  int repetition_index = 0;

  void parse_fen(const std::string &fen);
  std::string get_fen() const;
  U64 generate_hash_key() const;

  bool is_draw() const;

  inline UndoInfo make_move(Move m)
  {
    int from = move_from(m);
    int to = move_to(m);
    Piece prom = move_prom(m);

    UndoInfo undo = {hash_key, ep_square, castling_rights, board_sq[to], false, halfmove_clock};

    int us = side_to_move;
    int them = us ^ 1;
    Piece moving_piece = board_sq[from];

    hash_key ^= Zobrist::castling[castling_rights];
    if (ep_square != -1)
      hash_key ^= Zobrist::ep[ep_square % 8];
    hash_key ^= Zobrist::pieces[us][moving_piece][from];

    U64 move_mask = (1ULL << from) | (1ULL << to);
    piece_bb[us][moving_piece] ^= move_mask;
    occ[us] ^= move_mask;
    board_sq[from] = NONE;

    // Handle Captures
    if (moving_piece == PAWN && to == ep_square)
    {
      // En Passant Capture
      undo.is_ep = true;
      undo.captured_piece = PAWN;
      int cap_sq = to + (us == WHITE ? -8 : 8);
      piece_bb[them][PAWN] ^= (1ULL << cap_sq);
      occ[them] ^= (1ULL << cap_sq);
      board_sq[cap_sq] = NONE;
      hash_key ^= Zobrist::pieces[them][PAWN][cap_sq];
    }
    else if (undo.captured_piece != NONE)
    {
      // Normal Capture
      piece_bb[them][undo.captured_piece] ^= (1ULL << to);
      occ[them] ^= (1ULL << to);
      hash_key ^= Zobrist::pieces[them][undo.captured_piece][to];
    }

    // Handle Promotions
    Piece placed_piece = (prom != NONE) ? prom : moving_piece;
    if (prom != NONE)
    {
      piece_bb[us][moving_piece] ^= (1ULL << to); // Remove pawn
      piece_bb[us][placed_piece] ^= (1ULL << to); // Place promoted piece
    }

    // Update Mailbox and Hash
    board_sq[to] = placed_piece;
    hash_key ^= Zobrist::pieces[us][placed_piece][to]; // Hash in placed piece at 'to'

    // Handle Castling (Moving the Rook)
    if (moving_piece == KING && abs(from - to) == 2)
    {
      int r_from = (to == 6) ? 7 : (to == 2) ? 0
                                : (to == 62)  ? 63
                                              : 56;
      int r_to = (to == 6) ? 5 : (to == 2) ? 3
                              : (to == 62)  ? 61
                                            : 59;

      U64 rook_mask = (1ULL << r_from) | (1ULL << r_to);
      piece_bb[us][ROOK] ^= rook_mask;
      occ[us] ^= rook_mask;
      board_sq[r_from] = NONE;
      board_sq[r_to] = ROOK;

      hash_key ^= Zobrist::pieces[us][ROOK][r_from];
      hash_key ^= Zobrist::pieces[us][ROOK][r_to];
    }

    // Finalize Turn Updates
    occ[BOTH] = occ[WHITE] | occ[BLACK];
    castling_rights &= Bitboard::castling_board[from];
    castling_rights &= Bitboard::castling_board[to];

    // Set En Passant target if pawn double pushed
    // ep_square = (moving_piece == PAWN && abs(from - to) == 16) ? (from + to) / 2 : -1;

    ep_square = -1;
    if (moving_piece == PAWN && abs(from - to) == 16)
    {
      int ep_sq_candidate = (from + to) / 2;
      // int rank = ep_sq_candidate / 8;
      int file = ep_sq_candidate % 8;

      // FIDE Rule: Only set the EP square if an enemy pawn can actually capture it!
      bool can_be_captured = false;
      if (file > 0 && board_sq[to - 1] == PAWN && (piece_bb[them][PAWN] & (1ULL << (to - 1))))
        can_be_captured = true;
      if (file < 7 && board_sq[to + 1] == PAWN && (piece_bb[them][PAWN] & (1ULL << (to + 1))))
        can_be_captured = true;

      if (can_be_captured)
        ep_square = ep_sq_candidate;
    }

    // Hash in the new state
    hash_key ^= Zobrist::castling[castling_rights];
    if (ep_square != -1)
      hash_key ^= Zobrist::ep[ep_square % 8];
    hash_key ^= Zobrist::side;

    side_to_move ^= 1;
    halfmove_clock++;

    // Resets halfmove clock on pawn moves or captures
    if (moving_piece == PAWN || undo.captured_piece != NONE)
      halfmove_clock = 0;
    if (us == BLACK)
      fullmove_number++;

    // hash_key ^= (1ULL << from) ^ (1ULL << to);

    // if (repetition_index >= 2047)
    // {
    //     std::cout << "What the flipp? board.h:129" << std::endl;
    // }

    assert(repetition_index < 2047);

    repetition_table[repetition_index++] = hash_key;

    return undo;
  }

  inline void unmake_move(Move m, const UndoInfo &undo)
  {
    if (repetition_index > 0)
      repetition_index--;

    side_to_move ^= 1;
    int us = side_to_move;
    int them = us ^ 1;
    int from = move_from(m);
    int to = move_to(m);
    Piece prom = move_prom(m);

    Piece placed_piece = board_sq[to];
    Piece moving_piece = (prom != NONE) ? PAWN : placed_piece;

    U64 move_mask = (1ULL << from) | (1ULL << to);

    // Put piece back
    if (prom != NONE)
    {
      piece_bb[us][placed_piece] ^= (1ULL << to);
      piece_bb[us][moving_piece] ^= (1ULL << from);
    }
    else
    {
      piece_bb[us][moving_piece] ^= move_mask;
    }
    occ[us] ^= move_mask;

    board_sq[to] = (undo.is_ep) ? NONE : undo.captured_piece;
    board_sq[from] = moving_piece;

    // Restore captured piece
    if (undo.is_ep)
    {
      int cap_sq = to + (us == WHITE ? -8 : 8);
      piece_bb[them][PAWN] ^= (1ULL << cap_sq);
      occ[them] ^= (1ULL << cap_sq);
      board_sq[cap_sq] = PAWN;
    }
    else if (undo.captured_piece != NONE)
    {
      piece_bb[them][undo.captured_piece] ^= (1ULL << to);
      occ[them] ^= (1ULL << to);
    }

    // Move Rook back if castling
    if (moving_piece == KING && abs(from - to) == 2)
    {
      int r_from = (to == 6) ? 7 : (to == 2) ? 0
                                : (to == 62)  ? 63
                                              : 56;
      int r_to = (to == 6) ? 5 : (to == 2) ? 3
                              : (to == 62)  ? 61
                                            : 59;

      U64 rook_mask = (1ULL << r_from) | (1ULL << r_to);
      piece_bb[us][ROOK] ^= rook_mask;
      occ[us] ^= rook_mask;
      board_sq[r_from] = ROOK;
      board_sq[r_to] = NONE;
    }

    // Restore state variables
    occ[BOTH] = occ[WHITE] | occ[BLACK];
    castling_rights = undo.castling_rights;
    ep_square = undo.ep_square;
    hash_key = undo.hash_key;
    halfmove_clock = undo.halfmove_clock;
    if (us == BLACK)
      fullmove_number--;

    // Revert to the old Zobrist Hash perfectly
    hash_key = undo.hash_key;
  }

  inline UndoInfo make_null_move()
  {
    UndoInfo undo = {hash_key, ep_square, castling_rights, NONE, false, halfmove_clock};
    if (ep_square != -1)
      hash_key ^= Zobrist::ep[ep_square % 8];
    ep_square = -1;
    hash_key ^= Zobrist::side;
    side_to_move ^= 1;
    halfmove_clock++;
    repetition_table[repetition_index++] = hash_key;
    return undo;
  }

  inline void unmake_null_move(const UndoInfo &undo)
  {
    repetition_index--;
    side_to_move ^= 1;
    ep_square = undo.ep_square;
    hash_key = undo.hash_key;
    halfmove_clock = undo.halfmove_clock;
  }
};
