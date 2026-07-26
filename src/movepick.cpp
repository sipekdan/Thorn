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

#include <algorithm>

#include "movepick.h"
#include "evaluate.h"

const int mvv_lva[7] = {100, 320, 330, 500, 900, 20000, 0};

MovePicker::MovePicker(const Position& p, Move tm, const Move k[2], const int h[2][64][64], bool q)
  : pos(p), tt_move(tm), history(h), captures_only(q), stage(TT_STAGE), current_idx(0) {
  if (k != nullptr) {
    killers[0] = k[0];
    killers[1] = k[1];
  } else {
    killers[0] = killers[1] = 0;
  }
}

bool MovePicker::is_valid_move(Move m) {
  if (m == 0) return false;
  int from = move_from(m), to = move_to(m), us = pos.side_to_move;
  Piece p = pos.board_sq[from];
  if (p == NONE || (pos.piece_bb[us][p] & (1ULL << from)) == 0) return false;
  if (pos.occ[us] & (1ULL << to)) return false;
  return true;
}

void MovePicker::score_moves() {
  for (int i = 0; i < moves.count; i++) {
    Move m = moves.moves[i];
    int from = move_from(m), to = move_to(m);
    Piece attacker = pos.board_sq[from];
    Piece victim = pos.board_sq[to];

    if (victim != NONE) {
      scores[i] = 1000000 + (mvv_lva[victim] * 10) - mvv_lva[attacker];
    } else if (move_prom(m) != NONE) {
      scores[i] = 950000;
    } else if (m == killers[0]) {
      scores[i] = 900000;
    } else if (m == killers[1]) {
      scores[i] = 800000;
    } else if (history != nullptr) {
      scores[i] = history[pos.side_to_move][from][to];
    } else {
      // scores[i] = (history_table != nullptr) ? history_table[pos.side_to_move][from][to] : 0;
      scores[i] = 0;
    }
  }
}

Move MovePicker::get_best() {
  if (current_idx >= moves.count) return 0;
  int best_score = -9999999;
  int best_idx = current_idx;

  for (int i = current_idx; i < moves.count; i++) {
    if (scores[i] > best_score) {
      best_score = scores[i];
      best_idx = i;
    }
  }

  Move m = moves.moves[best_idx];
  std::swap(moves.moves[current_idx], moves.moves[best_idx]);
  std::swap(scores[current_idx], scores[best_idx]);
  current_idx++;
  return m;
}

Move MovePicker::next_move() {
  while (true) {
    switch (stage) {
      case TT_STAGE:
        stage = GEN_CAPTURES;
        if (tt_move && is_valid_move(tt_move)) return tt_move;
        break;

      case GEN_CAPTURES:
        moves.count = 0;
        Movegen::generate_tactical(pos, moves);
        score_moves();
        current_idx = 0;
        stage = CAPTURES;
        break;

      case CAPTURES: {
        Move m = get_best();
        if (m == 0) {
            if (captures_only) { stage = DONE; break; }
            stage = GEN_QUIETS;
            break;
        }
        if (m == tt_move) continue;
        return m;
      }

      case GEN_QUIETS: {
        moves.count = 0;
        MoveList pseudo;
        Movegen::generate_pseudo_moves(pos, pseudo);
        for (int i = 0; i < pseudo.count; i++) {
            Move m = pseudo.moves[i];
            if (pos.board_sq[move_to(m)] == NONE && move_prom(m) == NONE) {
                moves.push(move_from(m), move_to(m));
            }
        }
        score_moves();
        current_idx = 0;
        stage = QUIETS;
        break;
      }

      case QUIETS: {
        Move m = get_best();
        if (m == 0) { stage = DONE; break; }
        if (m == tt_move) continue;
        return m;
      }

      case DONE:
        return 0;
    }
  }
}
