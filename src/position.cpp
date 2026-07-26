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

#include <sstream>

#include "position.h"
#include "bitboard.h"

bool Position::is_draw() const
{
  // 1. Fifty-Move Rule (100 half-moves)
  if (halfmove_clock >= 100)
    return true;

  // 2. Threefold Repetition (Search Optimization)
  int limit = repetition_index - halfmove_clock - 1;
  if (limit < 0)
    limit = 0;

  for (int i = repetition_index - 2; i >= limit; i -= 2)
  {
    if (repetition_table[i] == hash_key)
    {
      return true;
    }
  }

  // 3. Strict Insufficient Material (FIDE Rules)
  // If any Pawns, Rooks, or Queens exist, mate is theoretically possible.
  if (piece_bb[WHITE][PAWN] || piece_bb[BLACK][PAWN] ||
      piece_bb[WHITE][ROOK] || piece_bb[BLACK][ROOK] ||
      piece_bb[WHITE][QUEEN] || piece_bb[BLACK][QUEEN])
  {
    return false;
  }

  // Count remaining minor pieces (Knights and Bishops)
  int white_knights = Bitboard::popcount(piece_bb[WHITE][KNIGHT]);
  int black_knights = Bitboard::popcount(piece_bb[BLACK][KNIGHT]);
  int white_bishops = Bitboard::popcount(piece_bb[WHITE][BISHOP]);
  int black_bishops = Bitboard::popcount(piece_bb[BLACK][BISHOP]);

  int white_minors = white_knights + white_bishops;
  int black_minors = black_knights + black_bishops;

  // King vs King
  if (white_minors == 0 && black_minors == 0)
    return true;

  // King + 1 Minor Piece vs King
  if (white_minors == 1 && black_minors == 0)
    return true;
  if (black_minors == 1 && white_minors == 0)
    return true;

  return false;
}

U64 Position::generate_hash_key() const
{
  U64 final_hash = 0;
  for (int sq = 0; sq < 64; sq++)
  {
    Piece p = board_sq[sq];
    if (p != NONE)
    {
      int color = (piece_bb[WHITE][p] & (1ULL << sq)) ? WHITE : BLACK;
      final_hash ^= Zobrist::pieces[color][p][sq];
    }
  }
  final_hash ^= Zobrist::castling[castling_rights];
  if (ep_square != -1)
    final_hash ^= Zobrist::ep[ep_square % 8];
  if (side_to_move == BLACK)
    final_hash ^= Zobrist::side;
  return final_hash;
}

void Position::parse_fen(const std::string &fen)
{
  for (int i = 0; i < 2; i++)
    for (int j = 0; j < 6; j++)
      piece_bb[i][j] = 0;
  for (int i = 0; i < 64; i++)
    board_sq[i] = NONE;
  occ[WHITE] = occ[BLACK] = occ[BOTH] = 0;
  castling_rights = 0;
  ep_square = -1;

  std::istringstream iss(fen);
  std::string b, c, cast, ep;
  iss >> b >> c >> cast >> ep;

  halfmove_clock = 0;
  fullmove_number = 1;
  if (iss >> halfmove_clock) {
    iss >> fullmove_number;
  }

  int sq = 56;
  for (char ch : b)
  {
    if (ch >= '1' && ch <= '8')
    {
      sq += (ch - '0');
      continue;
    }
    if (ch == '/')
    {
      sq -= 16;
      continue;
    }
    Color col = (ch >= 'a' && ch <= 'z') ? BLACK : WHITE;
    Piece p = NONE;
    char l = tolower(ch);

    if (l == 'p') p = PAWN;
    else if (l == 'n') p = KNIGHT;
    else if (l == 'b') p = BISHOP;
    else if (l == 'r') p = ROOK;
    else if (l == 'q') p = QUEEN;
    else if (l == 'k') p = KING;

    if (p != NONE)
    {
      Bitboard::set_bit(piece_bb[col][p], sq);
      Bitboard::set_bit(occ[col], sq);
      Bitboard::set_bit(occ[BOTH], sq);
      board_sq[sq] = p;
      sq++;
    }
  }
  side_to_move = (c == "w") ? WHITE : BLACK;
  for (char ch : cast)
  {
    if (ch == 'K')
      castling_rights |= WK_CASTLE;
    if (ch == 'Q')
      castling_rights |= WQ_CASTLE;
    if (ch == 'k')
      castling_rights |= BK_CASTLE;
    if (ch == 'q')
      castling_rights |= BQ_CASTLE;
  }

  // if (ep != "-") ep_square = (ep[1] - '1') * 8 + (ep[0] - 'a');

  ep_square = -1;
  if (ep != "-")
  {
    int ep_sq_candidate = (ep[1] - '1') * 8 + (ep[0] - 'a');
    int us = side_to_move;
    // int them = us ^ 1;

    // The FEN says there's an EP square. Let's verify an enemy pawn can actually take it.
    // The pawn that moved is "behind" the EP square from the perspective of the side to move
    int pawn_sq = ep_sq_candidate + (us == WHITE ? -8 : 8);
    int file = pawn_sq % 8;

    bool can_be_captured = false;
    if (file > 0 && board_sq[pawn_sq - 1] == PAWN && (piece_bb[us][PAWN] & (1ULL << (pawn_sq - 1))))
      can_be_captured = true;
    if (file < 7 && board_sq[pawn_sq + 1] == PAWN && (piece_bb[us][PAWN] & (1ULL << (pawn_sq + 1))))
      can_be_captured = true;

    if (can_be_captured)
      ep_square = ep_sq_candidate;
  }

  hash_key = generate_hash_key();
  repetition_index = 0;
  repetition_table[repetition_index++] = hash_key;
}

std::string Position::get_fen() const
{
  std::string fen = "";
  int empty = 0;
  for (int r = 7; r >= 0; r--)
  {
    for (int f = 0; f < 8; f++)
    {
      int sq = r * 8 + f;
      Piece p = board_sq[sq];
      if (p == NONE)
      {
        empty++;
      }
      else
      {
        if (empty > 0)
        {
            fen += std::to_string(empty);
            empty = 0;
        }
        bool is_white = (piece_bb[WHITE][p] & (1ULL << sq));
        char c = "?pnbrqk"[p + 1];
        fen += is_white ? toupper(c) : c;
      }
    }
    if (empty > 0)
    {
      fen += std::to_string(empty);
      empty = 0;
    }
    if (r > 0)
      fen += "/";
  }
  fen += (side_to_move == WHITE) ? " w " : " b ";
  std::string cast = "";
  if (castling_rights & WK_CASTLE)
    cast += "K";
  if (castling_rights & WQ_CASTLE)
    cast += "Q";
  if (castling_rights & BK_CASTLE)
    cast += "k";
  if (castling_rights & BQ_CASTLE)
    cast += "q";
  fen += cast.empty() ? "-" : cast;
  if (ep_square != -1)
  {
    fen += " ";
    fen += (char)('a' + (ep_square % 8));
    fen += (char)('1' + (ep_square / 8));
  }
  else
  {
    fen += " -";
  }
  fen += " " + std::to_string(halfmove_clock) + " " + std::to_string(fullmove_number);
  return fen;
}