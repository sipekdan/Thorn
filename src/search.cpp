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

#include <iostream>
#include <algorithm>
#include <cmath>

#include "search.h"
// #include "nnue.h"
#include "evaluate.h"
#include "movepick.h"
#include "movegen.h"
#include "tt.h"
#include "threads.h"

namespace Search {

  const int see_values[7] = {100, 320, 330, 500, 900, 20000, 0};
  int lmr_table[64][64];
  bool lmr_initialized = false;

  void init() {
    for (int depth = 1; depth < 64; ++depth) {
      for (int moves = 1; moves < 64; ++moves) {
        lmr_table[depth][moves] = 1 + int(std::log(depth) * std::log(moves) / 2.25);
      }
    }
    lmr_initialized = true;
  }

  /// get_least_valuable_attacker() uses bitboards to efficiently find the lowest
  /// value piece attacking a given square. Used by SEE.

  int get_least_valuable_attacker(const Position& pos, int sq, U64 occupied, int color, int& att_sq) {
    U64 pawns = Bitboard::pawn_attacks[color ^ 1][sq] & pos.piece_bb[color][PAWN] & occupied;
    if (pawns) { att_sq = Bitboard::lsb(pawns); return PAWN; }

    U64 knights = Bitboard::knight_attacks[sq] & pos.piece_bb[color][KNIGHT] & occupied;
    if (knights) { att_sq = Bitboard::lsb(knights); return KNIGHT; }

    U64 bishops = Bitboard::get_bishop_attacks(sq, occupied) & pos.piece_bb[color][BISHOP] & occupied;
    if (bishops) { att_sq = Bitboard::lsb(bishops); return BISHOP; }

    U64 rooks = Bitboard::get_rook_attacks(sq, occupied) & pos.piece_bb[color][ROOK] & occupied;
    if (rooks) { att_sq = Bitboard::lsb(rooks); return ROOK; }

    U64 queens = Bitboard::get_queen_attacks(sq, occupied) & pos.piece_bb[color][QUEEN] & occupied;
    if (queens) { att_sq = Bitboard::lsb(queens); return QUEEN; }

    U64 kings = Bitboard::king_attacks[sq] & pos.piece_bb[color][KING] & occupied;
    if (kings) { att_sq = Bitboard::lsb(kings); return KING; }

    return NONE;
  }

  /// see_capture() performs Static Exchange Evaluation. It simulates a sequence 
  /// of captures on a single square to determine if a capture is materially profitable.

  bool see_capture(const Position& pos, Move m) {
    int from = move_from(m);
    int to = move_to(m);
    int next_victim = pos.board_sq[from];
    int balance = 0;

    if (pos.board_sq[to] != NONE) {
      balance = see_values[pos.board_sq[to]];
    } else if (next_victim == PAWN && from % 8 != to % 8) {
      balance = see_values[PAWN]; // En-passant
    }

    Piece prom = move_prom(m);
    if (prom != NONE) {
      balance += see_values[prom] - see_values[PAWN];
      next_victim = prom;
    }

    // Quick check: If the captured piece is equal or more valuable than the attacker, it's always safe
    if (balance >= see_values[next_victim] - 50) return true;

    int color = pos.side_to_move ^ 1;
    U64 occupied = pos.occ[BOTH] ^ (1ULL << from);

    int gain[32];
    int d = 0;
    gain[d] = balance;

    while (d < 31) {
      int att_sq;
      int att_piece = get_least_valuable_attacker(pos, to, occupied, color, att_sq);
      if (att_piece == NONE) break;

      occupied ^= (1ULL << att_sq);
      
      d++;
      gain[d] = see_values[next_victim] - gain[d - 1];
      next_victim = att_piece;
      color ^= 1;
    }

    // Minimax the capture tree backwards to find the true outcome
    while (--d > 0) {
      gain[d - 1] = -std::max(-gain[d - 1], gain[d]);
    }

    return gain[0] >= 0;
  }

  /// format_score() converts internal score representations into standard
  /// UCI format for the GUI (e.g., "cp 50", "mate 3").

  std::string format_score(int score) {
    if (score > MATE_SCORE - 100) {
      int plies = MATE_SCORE - score;
      return "mate " + std::to_string((plies + 1) / 2);
    } else if (score < -MATE_SCORE + 100) {
      int plies = MATE_SCORE + score;
      return "mate " + std::to_string(-(plies + 1) / 2);
    }
    return "cp " + std::to_string(score);
  }

  int quiescence(Position& pos, int alpha, int beta, int ply, uint64_t& local_nodes, int& seldepth/*, const NNUE::Accumulator& acc*/) {
    if ((local_nodes & 2047) == 0 && Threads::check_time()) return 0;
    local_nodes++;
    if (ply > seldepth) seldepth = ply;

    if (ply >= 128) return Eval::evaluate(pos/*, acc*/);

    int stand_pat = Eval::evaluate(pos/*, acc*/);
    if (stand_pat >= beta) return beta;
    if (stand_pat > alpha) alpha = stand_pat;

    MovePicker picker(pos, 0, nullptr, nullptr, true/*!in_check*/);
    Move move;
    // int legal_moves = 0;

    while ((move = picker.next_move()) != 0) {
      if (!see_capture(pos, move)) {
          continue; 
      }
      
      // Delta pruning
      int captured = pos.board_sq[move_to(move)];
      if (pos.board_sq[move_from(move)] == PAWN && move_to(move) == pos.ep_square) {
        captured = PAWN;
      }

      int prom = move_prom(move);
      int gain = (captured != NONE ? see_values[captured] : 0) + (prom != NONE ? see_values[prom] - see_values[PAWN] : 0);

      // If capturing this piece + a 200cp safety margin STILL doesn't beat alpha, prune it!
      if (stand_pat + gain + 200 < alpha) {
        continue;
      }

      // NNUE::Accumulator next_acc = NNUE::get_next_accumulator(pos, move, acc);
      UndoInfo undo = pos.make_move(move);

      if (Movegen::is_in_check(pos, pos.side_to_move ^ 1)) {
        pos.unmake_move(move, undo);
        continue;
      }

      // legal_moves++;
      int score = -quiescence(pos, -beta, -alpha, ply + 1, local_nodes, seldepth/*, next_acc*/);
      pos.unmake_move(move, undo);

      if (Threads::stop_flag.load(std::memory_order_relaxed)) return 0;
      if (score >= beta) return beta;
      if (score > alpha) alpha = score;
    }

    // if (in_check && legal_moves == 0) {
    //     return -MATE_SCORE + ply;
    // }

    return alpha;
  }

  /// negamax() is the core alpha-beta search algorithm. It recursively searches
  /// the game tree using TT, NMP, LMR, and history heuristics.

  int negamax(Position& pos, int depth, int alpha, int beta, int ply, uint64_t& local_nodes, int& seldepth, Move* best_root_move, Move killer_moves[128][2], int history[2][64][64]/*, const NNUE::Accumulator& acc*/) {
    if (ply > 0 && pos.is_draw()) return 0;
    if ((local_nodes & 2047) == 0 && Threads::check_time()) return 0;
    local_nodes++;

    if (ply > seldepth) seldepth = ply;
    if (ply >= 128) return Eval::evaluate(pos/*, acc*/);
    if (depth <= 0) return quiescence(pos, alpha, beta, ply, local_nodes, seldepth/*, acc*/);

    bool in_check = Movegen::is_in_check(pos, pos.side_to_move);

    // Reverse Futility Pruning
    if (depth <= 5 && !in_check && std::abs(beta) < MATE_SCORE - 100) {
      int static_eval = Eval::evaluate(pos/*, acc*/);
      int rfp_margin = 120 * depth; 
      if (static_eval - rfp_margin >= beta) {
          return static_eval;
      }
    }

    // Null Move Pruning
    if (depth >= 3 && !in_check && ply > 0 && std::abs(beta) < MATE_SCORE - 100) {
      U64 non_pawn_material = pos.piece_bb[pos.side_to_move][KNIGHT] |
                              pos.piece_bb[pos.side_to_move][BISHOP] |
                              pos.piece_bb[pos.side_to_move][ROOK]   |
                              pos.piece_bb[pos.side_to_move][QUEEN];

      if (non_pawn_material) {
        UndoInfo null_undo = pos.make_null_move();
        int R = 2 + (depth / 4);
        int null_score = -negamax(pos, depth - 1 - R, -beta, -beta + 1, ply + 1, local_nodes, seldepth, nullptr, killer_moves, history/*, acc*/);
        pos.unmake_null_move(null_undo);

        if (Threads::stop_flag.load(std::memory_order_relaxed)) return 0;

        if (null_score >= beta) {
          return null_score >= MATE_SCORE - 100 ? beta : null_score;
        }
      }
    }

    Move tt_move = 0;
    int tt_score = 0;
    bool tt_cutoff = TT::probe(pos.hash_key, depth, alpha, beta, ply, tt_score, tt_move);
    if (tt_cutoff && ply > 0) {
      return tt_score;
    }

    MovePicker picker(pos, tt_move, killer_moves[ply], history, false);
    Move move;
    Move best_move = 0;
    int best_score = -INFINITY_SCORE;
    int original_alpha = alpha;
    int legal_moves = 0;

    while ((move = picker.next_move()) != 0) {
      // NNUE::Accumulator next_acc = NNUE::get_next_accumulator(pos, move, acc);
      bool is_tactical = (pos.board_sq[move_to(move)] != NONE) || (move_prom(move) != NONE);

      UndoInfo undo = pos.make_move(move);

      if (Movegen::is_in_check(pos, pos.side_to_move ^ 1)) {
        pos.unmake_move(move, undo);
        continue;
      }

      legal_moves++;
      int score; // int score = -negamax(pos, depth - 1, -beta, -alpha, ply + 1, local_nodes, seldepth, nullptr, next_acc);
      bool gives_check = Movegen::is_in_check(pos, pos.side_to_move);
      int extension = gives_check ? 1 : 0;
      int next_depth = depth - 1 + extension;

      // Principal Variation Search & LMR
      if (legal_moves == 1) {
        score = -negamax(pos, next_depth, -beta, -alpha, ply + 1, local_nodes, seldepth, nullptr, killer_moves, history/*, next_acc*/);
      } else {
        if (depth >= 3 && legal_moves >= 4 && !is_tactical && !gives_check/* && !in_check*/) {
          // int reduction = (legal_moves > 10) ? 2 : 1;
          int d_idx = std::min(depth, 63);
          int m_idx = std::min(legal_moves, 63);
          int reduction = lmr_table[d_idx][m_idx];

          score = -negamax(pos, next_depth - reduction, -alpha - 1, -alpha, ply + 1, local_nodes, seldepth, nullptr, killer_moves, history/*, next_acc*/);
        } else {
          score = alpha + 1; // Hack to force full-depth search if not reduced
        }

        if (score > alpha) {
          score = -negamax(pos, next_depth, -alpha - 1, -alpha, ply + 1, local_nodes, seldepth, nullptr, killer_moves, history/*, next_acc*/);

          if (score > alpha && score < beta) {
            score = -negamax(pos, next_depth, -beta, -alpha, ply + 1, local_nodes, seldepth, nullptr, killer_moves, history/*, next_acc*/);
          }
        }
      }

      pos.unmake_move(move, undo);

      if (Threads::stop_flag.load(std::memory_order_relaxed)) return 0;

      if (score > best_score) {
        best_score = score;
        best_move = move;
        if (ply == 0 && best_root_move) *best_root_move = move;
      }

      if (best_score > alpha) alpha = best_score;
      if (alpha >= beta) {
        if (!is_tactical) {
          if (move != killer_moves[ply][0]) {
            killer_moves[ply][1] = killer_moves[ply][0]; 
            killer_moves[ply][0] = move;                 
          }
          // history[pos.side_to_move][move_from(move)][move_to(move)] += depth * depth;

          // int bonus = depth * depth;
          int bonus = std::min(depth * depth, 400);// Cap history bonus
          int& hist = history[pos.side_to_move][move_from(move)][move_to(move)];
          hist += bonus - (hist * bonus) / 8192;
        }
        break;
      }
    }

    if (legal_moves == 0) {
      bool in_check = Movegen::is_in_check(pos, pos.side_to_move);
      return in_check ? -MATE_SCORE + ply : 0;
    }

    if (!Threads::stop_flag.load(std::memory_order_relaxed)) {
      TT::Flag flag = (best_score <= original_alpha) ? TT::UPPER :
                      (best_score >= beta) ? TT::LOWER : TT::EXACT;
      TT::store(pos.hash_key, depth, best_score, flag, best_move, ply);
    }

    return best_score;
  }

  /// search_position() is the entry point for the thread search. It utilizes
  /// iterative deepening and aspiration windows to control negamax limits.

  void search_position(Position& pos, int target_depth, bool is_main_thread) {
    uint64_t local_nodes = 0;
    Move best_move = 0;
    Move ponder_move = 0;
    int seldepth = 0;

    Move killer_moves[128][2] = {{0}};
    int history_table[2][64][64] = {{{0}}};

    // NNUE::Accumulator root_acc;
    // if (NNUE::is_loaded) {
    //     NNUE::refresh_accumulator(pos, root_acc);
    // }

    int prev_score = 0;

    for (int d = 1; d <= target_depth; d++) {
      Move current_best = 0;
      seldepth = 0;
      int score; // = negamax(pos, d, -INFINITY_SCORE, INFINITY_SCORE, 0, local_nodes, seldepth, &current_best, killer_moves, history_table, root_acc);

      int alpha = -INFINITY_SCORE;
      int beta = INFINITY_SCORE;
      int delta = 50;

      if (d >= 4) {
        alpha = std::max(-INFINITY_SCORE, prev_score - delta);
        beta = std::min(INFINITY_SCORE, prev_score + delta);
      }

      while (true) {
        score = negamax(pos, d, alpha, beta, 0, local_nodes, seldepth, &current_best, killer_moves, history_table/*, root_acc*/);

        if (Threads::stop_flag.load(std::memory_order_relaxed)) break;

        if (score <= alpha) {
          // Fail low! Score is worse than expected. Widen the lower bound and try again.
          alpha = std::max(-INFINITY_SCORE, alpha - delta);
          delta *= 2; 
        } else if (score >= beta) {
          // Fail high! Score is better than expected. Widen the upper bound and try again.
          beta = std::min(INFINITY_SCORE, beta + delta);
          delta *= 2; 
        } else {
          // Score is perfectly within the window!
          break;
        }
      }

      if (Threads::stop_flag.load(std::memory_order_relaxed)) break;

      prev_score = score;
      if (current_best != 0) best_move = current_best;

      if (is_main_thread) {
        Threads::global_nodes.fetch_add(local_nodes, std::memory_order_relaxed);
        uint64_t total_nodes = Threads::global_nodes.load(std::memory_order_relaxed);
        local_nodes = 0; // Reset local accumulator

        int64_t elapsed = Threads::get_elapsed_ms();
        uint64_t nps = (elapsed > 0) ? (total_nodes * 1000) / elapsed : 0;
        int hashfull_val = TT::hashfull();

        std::string pv_str = "";
        ponder_move = 0;
        Position pv_pos = pos;

        for (int i = 0; i < d; i++) {
          Move pv_move = 0;

          if (i == 0) {
            pv_move = best_move;
          } else {
            int dummy_score;
            TT::probe(pv_pos.hash_key, 0, -INFINITY_SCORE, INFINITY_SCORE, 0, dummy_score, pv_move);
          }
            
          if (pv_move == 0) break;

          // Verify legal pseudo-move
          MoveList list;
          Movegen::generate_pseudo_moves(pv_pos, list);
          bool is_valid = false;
          for(int j = 0; j < list.count; j++) {
              if (list.moves[j] == pv_move) { is_valid = true; break; }
          }
          if (!is_valid) break;

          UndoInfo undo = pv_pos.make_move(pv_move);
          if (Movegen::is_in_check(pv_pos, pv_pos.side_to_move ^ 1)) {
              pv_pos.unmake_move(pv_move, undo);
              break;
          }

          pv_str += move_to_string(pv_move) + " ";
          if (i == 1) ponder_move = pv_move;
        }

        std::cout << "info depth " << d
                  << " seldepth " << seldepth
                  << " score " << format_score(score)
                  << " time " << elapsed
                  << " nodes " << total_nodes
                  << " nps " << nps
                  << " hashfull " << hashfull_val
                  << " pv " << pv_str << std::endl;
      }
    }

    if (is_main_thread) {
      std::cout << "bestmove " << move_to_string(best_move);
      if (ponder_move != 0) {
        std::cout << " ponder " << move_to_string(ponder_move);
      }
      std::cout << std::endl;
    } else {
      Threads::global_nodes.fetch_add(local_nodes, std::memory_order_relaxed); 
    }
  }
}
