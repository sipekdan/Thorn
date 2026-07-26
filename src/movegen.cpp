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

#include "movegen.h"

namespace Movegen {
  void generate_legal_moves(const Position &pos, MoveList &moves) {
    moves.count = 0;

    int us = pos.side_to_move;
    int them = us ^ 1;

    if (pos.piece_bb[us][KING] == 0)
      return;

    const int king_sq = Bitboard::lsb(pos.piece_bb[us][KING]);
    const U64 our = pos.occ[us];
    const U64 their = pos.occ[them];
    const U64 all = pos.occ[BOTH];
    const U64 block_no_k = all ^ (1ULL << king_sq);
    const U64 enemy_no_king = their & ~pos.piece_bb[them][KING];

    U64 enemy_attacks = 0;
    U64 p = pos.piece_bb[them][PAWN];

    if (them == WHITE) {
      enemy_attacks |= (p << 7) & 0x7F7F7F7F7F7F7F7FULL;
      enemy_attacks |= (p << 9) & 0xFEFEFEFEFEFEFEFEULL;
    }
    else {
      enemy_attacks |= (p >> 9) & 0x7F7F7F7F7F7F7F7FULL;
      enemy_attacks |= (p >> 7) & 0xFEFEFEFEFEFEFEFEULL;
    }

    p = pos.piece_bb[them][KNIGHT];
    while (p)
      enemy_attacks |= Bitboard::knight_attacks[Bitboard::pop_lsb(p)];

    p = pos.piece_bb[them][BISHOP] | pos.piece_bb[them][QUEEN];
    while (p)
      enemy_attacks |= Bitboard::get_bishop_attacks(Bitboard::pop_lsb(p), block_no_k);

    p = pos.piece_bb[them][ROOK] | pos.piece_bb[them][QUEEN];
    while (p)
      enemy_attacks |= Bitboard::get_rook_attacks(Bitboard::pop_lsb(p), block_no_k);

    if (pos.piece_bb[them][KING])
    {
      enemy_attacks |= Bitboard::king_attacks[Bitboard::lsb(pos.piece_bb[them][KING])];
    }

    const U64 checkers = (Bitboard::pawn_attacks[us][king_sq] & pos.piece_bb[them][PAWN]) |
                          (Bitboard::knight_attacks[king_sq] & pos.piece_bb[them][KNIGHT]) |
                          (Bitboard::get_bishop_attacks(king_sq, all) & (pos.piece_bb[them][BISHOP] | pos.piece_bb[them][QUEEN])) |
                          (Bitboard::get_rook_attacks(king_sq, all) & (pos.piece_bb[them][ROOK] | pos.piece_bb[them][QUEEN]));

    U64 check_mask = 0xFFFFFFFFFFFFFFFFULL;
    if (checkers)
    {
      if (checkers & (checkers - 1))
      {
        check_mask = 0; // Double check, king must move
      }
      else
      {
        int checker_sq = Bitboard::lsb(checkers);
        check_mask = (1ULL << checker_sq) | Bitboard::between_bb[king_sq][checker_sq];
      }
    }

    U64 pinned = 0;
    U64 pin_masks[64] = {0};

    U64 pinners = (Bitboard::get_bishop_attacks(king_sq, their) & (pos.piece_bb[them][BISHOP] | pos.piece_bb[them][QUEEN])) |
                  (Bitboard::get_rook_attacks(king_sq, their) & (pos.piece_bb[them][ROOK] | pos.piece_bb[them][QUEEN]));

    while (pinners)
    {
      int p_sq = Bitboard::pop_lsb(pinners);
      U64 path = Bitboard::between_bb[king_sq][p_sq] | (1ULL << p_sq);
      U64 blockers = path & our;

      if (blockers && !(blockers & (blockers - 1)))
      {
        int pinned_sq = Bitboard::lsb(blockers);
        pinned |= (1ULL << pinned_sq);
        pin_masks[pinned_sq] = path;
      }
    }

    const U64 valid = ~our & check_mask & ~pos.piece_bb[them][KING];
    U64 k_moves = Bitboard::king_attacks[king_sq] & ~our & ~enemy_attacks & ~pos.piece_bb[them][KING];

    while (k_moves)
      moves.push(king_sq, Bitboard::pop_lsb(k_moves));

    // --- Castling logic using enemy_attacks! ---
    if (!checkers)
    {
      if (us == WHITE)
      {
        if ((pos.castling_rights & WK_CASTLE) && !(all & 0x60ULL) && !(enemy_attacks & 0x70ULL))
          moves.push(4, 6);
        if ((pos.castling_rights & WQ_CASTLE) && !(all & 0xEULL) && !(enemy_attacks & 0x1CULL))
          moves.push(4, 2);
      }
      else
      {
        if ((pos.castling_rights & BK_CASTLE) && !(all & 0x6000000000000000ULL) && !(enemy_attacks & 0x7000000000000000ULL))
          moves.push(60, 62);
        if ((pos.castling_rights & BQ_CASTLE) && !(all & 0x0E00000000000000ULL) && !(enemy_attacks & 0x1C00000000000000ULL))
          moves.push(60, 58);
      }
    }

    if (checkers & (checkers - 1))
      return;

    p = pos.piece_bb[us][KNIGHT];
    while (p)
    {
      int sq = Bitboard::pop_lsb(p);
      U64 m = Bitboard::knight_attacks[sq] & valid;
      if (pinned & (1ULL << sq))
        m &= pin_masks[sq];
      while (m)
        moves.push(sq, Bitboard::pop_lsb(m));
    }

    p = pos.piece_bb[us][BISHOP] | pos.piece_bb[us][QUEEN];
    while (p)
    {
      int sq = Bitboard::pop_lsb(p);
      U64 m = Bitboard::get_bishop_attacks(sq, all) & valid;
      if (pinned & (1ULL << sq))
        m &= pin_masks[sq];
      while (m)
        moves.push(sq, Bitboard::pop_lsb(m));
    }

    p = pos.piece_bb[us][ROOK] | pos.piece_bb[us][QUEEN];
    while (p)
    {
      int sq = Bitboard::pop_lsb(p);
      U64 m = Bitboard::get_rook_attacks(sq, all) & valid;
      if (pinned & (1ULL << sq))
        m &= pin_masks[sq];
      while (m)
        moves.push(sq, Bitboard::pop_lsb(m));
    }

    U64 pawns = pos.piece_bb[us][PAWN];
    const int push_dir = (us == WHITE) ? 8 : -8;
    const int prom_rank = (us == WHITE) ? 7 : 0;
    const int start_rank = (us == WHITE) ? 1 : 6;

    while (pawns)
    {
      int sq = Bitboard::pop_lsb(pawns);
      U64 p_valid = valid;
      if (pinned & (1ULL << sq))
        p_valid &= pin_masks[sq];

      int p_sq = sq + push_dir;
      if (!(all & (1ULL << p_sq)))
      {
        if ((1ULL << p_sq) & p_valid)
        {
          if (p_sq / 8 == prom_rank)
          {
            moves.push(sq, p_sq, QUEEN);
            moves.push(sq, p_sq, ROOK);
            moves.push(sq, p_sq, BISHOP);
            moves.push(sq, p_sq, KNIGHT);
          }
          else
          {
            moves.push(sq, p_sq);
          }
        }
        int d_push = p_sq + push_dir;
        if ((sq / 8 == start_rank) && !(all & (1ULL << d_push)) && ((1ULL << d_push) & p_valid))
        {
          moves.push(sq, d_push);
        }
      }

      U64 caps = Bitboard::pawn_attacks[us][sq] & enemy_no_king & p_valid;
      while (caps)
      {
        int c_sq = Bitboard::pop_lsb(caps);
        if (c_sq / 8 == prom_rank)
        {
          moves.push(sq, c_sq, QUEEN);
          moves.push(sq, c_sq, ROOK);
          moves.push(sq, c_sq, BISHOP);
          moves.push(sq, c_sq, KNIGHT);
        }
        else
        {
          moves.push(sq, c_sq);
        }
      }

      if (pos.ep_square != -1 && (Bitboard::pawn_attacks[us][sq] & (1ULL << pos.ep_square)))
      {
        // if ((pin_masks[sq] & (1ULL << pos.ep_square)) || !(pinned & (1ULL << sq)))
        if (!(pinned & (1ULL << sq)) || (pin_masks[sq] & (1ULL << pos.ep_square)))
        {
          bool ep_resolves = !checkers ||
                              ((1ULL << (pos.ep_square - push_dir)) & check_mask) ||
                              ((1ULL << pos.ep_square) & check_mask);
          if (ep_resolves)
          {
            U64 t_occ = (all ^ (1ULL << sq) ^ (1ULL << (pos.ep_square - push_dir))) | (1ULL << pos.ep_square);
            if (!(Bitboard::get_rook_attacks(king_sq, t_occ) & (pos.piece_bb[them][ROOK] | pos.piece_bb[them][QUEEN])) &&
                !(Bitboard::get_bishop_attacks(king_sq, t_occ) & (pos.piece_bb[them][BISHOP] | pos.piece_bb[them][QUEEN])))
            {
              moves.push(sq, pos.ep_square);
            }
          }
        }
      }
    }
  }

  void generate_pseudo_moves(const Position &pos, MoveList &moves)
  {
      moves.count = 0;
      int us = pos.side_to_move;
      int them = us ^ 1;

      const U64 our = pos.occ[us];
      const U64 their = pos.occ[them];
      const U64 all = pos.occ[BOTH];

      if (pos.piece_bb[us][KING] == 0)
          return;
      const int king_sq = Bitboard::lsb(pos.piece_bb[us][KING]);

      // 1. King Moves (Normal steps only)
      U64 k_moves = Bitboard::king_attacks[king_sq] & ~our;
      while (k_moves)
          moves.push(king_sq, Bitboard::pop_lsb(k_moves));

      // 2. Castling (Check emptiness. Legality is handled in search)
      if (us == WHITE)
      {
          if ((pos.castling_rights & WK_CASTLE) && !(all & 0x60ULL))
          {
              if (!is_square_attacked(pos, 4, BLACK) && !is_square_attacked(pos, 5, BLACK))
                  moves.push(4, 6);
          }
          if ((pos.castling_rights & WQ_CASTLE) && !(all & 0xEULL))
          {
              if (!is_square_attacked(pos, 4, BLACK) && !is_square_attacked(pos, 3, BLACK))
                  moves.push(4, 2);
          }
      }
      else
      {
          if ((pos.castling_rights & BK_CASTLE) && !(all & 0x6000000000000000ULL))
          {
              if (!is_square_attacked(pos, 60, WHITE) && !is_square_attacked(pos, 61, WHITE))
                  moves.push(60, 62);
          }
          if ((pos.castling_rights & BQ_CASTLE) && !(all & 0x0E00000000000000ULL))
          {
              if (!is_square_attacked(pos, 60, WHITE) && !is_square_attacked(pos, 59, WHITE))
                  moves.push(60, 58);
          }
      }

      // 3. Knights
      U64 p = pos.piece_bb[us][KNIGHT];
      while (p)
      {
          int sq = Bitboard::pop_lsb(p);
          U64 m = Bitboard::knight_attacks[sq] & ~our;
          while (m)
              moves.push(sq, Bitboard::pop_lsb(m));
      }

      // 4. Bishops / Queens
      p = pos.piece_bb[us][BISHOP] | pos.piece_bb[us][QUEEN];
      while (p)
      {
          int sq = Bitboard::pop_lsb(p);
          U64 m = Bitboard::get_bishop_attacks(sq, all) & ~our;
          while (m)
              moves.push(sq, Bitboard::pop_lsb(m));
      }

      // 5. Rooks / Queens
      p = pos.piece_bb[us][ROOK] | pos.piece_bb[us][QUEEN];
      while (p)
      {
          int sq = Bitboard::pop_lsb(p);
          U64 m = Bitboard::get_rook_attacks(sq, all) & ~our;
          while (m)
              moves.push(sq, Bitboard::pop_lsb(m));
      }

      // 6. Pawns
      U64 pawns = pos.piece_bb[us][PAWN];
      const int push_dir = (us == WHITE) ? 8 : -8;
      const int prom_rank = (us == WHITE) ? 7 : 0;
      const int start_rank = (us == WHITE) ? 1 : 6;

      while (pawns)
      {
          int sq = Bitboard::pop_lsb(pawns);

          // Single Push
          int p_sq = sq + push_dir;
          if (!(all & (1ULL << p_sq)))
          {
              if (p_sq / 8 == prom_rank)
              {
                  moves.push(sq, p_sq, QUEEN);
                  moves.push(sq, p_sq, ROOK);
                  moves.push(sq, p_sq, BISHOP);
                  moves.push(sq, p_sq, KNIGHT);
              }
              else
              {
                  moves.push(sq, p_sq);
                  // Double Push
                  int d_push = p_sq + push_dir;
                  if ((sq / 8 == start_rank) && !(all & (1ULL << d_push)))
                  {
                      moves.push(sq, d_push);
                  }
              }
          }

          // Captures
          U64 caps = Bitboard::pawn_attacks[us][sq] & their;
          while (caps)
          {
              int c_sq = Bitboard::pop_lsb(caps);
              if (c_sq / 8 == prom_rank)
              {
                  moves.push(sq, c_sq, QUEEN);
                  moves.push(sq, c_sq, ROOK);
                  moves.push(sq, c_sq, BISHOP);
                  moves.push(sq, c_sq, KNIGHT);
              }
              else
              {
                  moves.push(sq, c_sq);
              }
          }

          // En Passant
          if (pos.ep_square != -1 && (Bitboard::pawn_attacks[us][sq] & (1ULL << pos.ep_square)))
          {
              moves.push(sq, pos.ep_square);
          }
      }
  }

  void generate_tactical(const Position &pos, MoveList &moves)
  {
      moves.count = 0;
      int us = pos.side_to_move, them = us ^ 1;
      const U64 their = pos.occ[them], all = pos.occ[BOTH];

      if (pos.piece_bb[us][KING] == 0)
          return;

      U64 pawns = pos.piece_bb[us][PAWN];
      const int push_dir = (us == WHITE) ? 8 : -8;
      const int prom_rank = (us == WHITE) ? 7 : 0;

      while (pawns)
      {
          int sq = Bitboard::pop_lsb(pawns);

          // Queen Promotions (Pushes)
          int p_sq = sq + push_dir;
          if (!(all & (1ULL << p_sq)) && p_sq / 8 == prom_rank)
          {
              moves.push(sq, p_sq, QUEEN);
          }

          // Captures
          U64 caps = Bitboard::pawn_attacks[us][sq] & their;
          while (caps)
          {
              int c_sq = Bitboard::pop_lsb(caps);
              if (c_sq / 8 == prom_rank)
                  moves.push(sq, c_sq, QUEEN); // Promo capture
              else
                  moves.push(sq, c_sq);
          }

          // En Passant
          if (pos.ep_square != -1 && (Bitboard::pawn_attacks[us][sq] & (1ULL << pos.ep_square)))
          {
              moves.push(sq, pos.ep_square);
          }
      }

      // Piece Captures
      U64 p = pos.piece_bb[us][KNIGHT];
      while (p)
      {
          int sq = Bitboard::pop_lsb(p);
          U64 m = Bitboard::knight_attacks[sq] & their;
          while (m)
              moves.push(sq, Bitboard::pop_lsb(m));
      }

      p = pos.piece_bb[us][BISHOP] | pos.piece_bb[us][QUEEN];
      while (p)
      {
          int sq = Bitboard::pop_lsb(p);
          U64 m = Bitboard::get_bishop_attacks(sq, all) & their;
          while (m)
              moves.push(sq, Bitboard::pop_lsb(m));
      }

      p = pos.piece_bb[us][ROOK] | pos.piece_bb[us][QUEEN];
      while (p)
      {
          int sq = Bitboard::pop_lsb(p);
          U64 m = Bitboard::get_rook_attacks(sq, all) & their;
          while (m)
              moves.push(sq, Bitboard::pop_lsb(m));
      }

      const int king_sq = Bitboard::lsb(pos.piece_bb[us][KING]);
      U64 k_moves = Bitboard::king_attacks[king_sq] & their;
      while (k_moves)
          moves.push(king_sq, Bitboard::pop_lsb(k_moves));
  }

  bool is_square_attacked(const Position &pos, int sq, int attacker_color)
  {
      const U64 all = pos.occ[BOTH];
      const int defender_color = attacker_color ^ 1;
      if (Bitboard::pawn_attacks[defender_color][sq] & pos.piece_bb[attacker_color][PAWN])
        return true;
      if (Bitboard::knight_attacks[sq] & pos.piece_bb[attacker_color][KNIGHT])
        return true;
      if (Bitboard::king_attacks[sq] & pos.piece_bb[attacker_color][KING])
        return true;
      if (Bitboard::get_bishop_attacks(sq, all) & (pos.piece_bb[attacker_color][BISHOP] | pos.piece_bb[attacker_color][QUEEN]))
        return true;
      if (Bitboard::get_rook_attacks(sq, all) & (pos.piece_bb[attacker_color][ROOK] | pos.piece_bb[attacker_color][QUEEN]))
        return true;
      return false;
  }

  bool is_in_check(const Position &pos, int color)
  {
    if (pos.piece_bb[color][KING] == 0)
      return false;
    return is_square_attacked(pos, Bitboard::lsb(pos.piece_bb[color][KING]), color ^ 1);
  }
}
