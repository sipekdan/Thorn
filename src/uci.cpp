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
#include <sstream>
#include <fstream>

#include "uci.h"
#include "position.h"
#include "evaluate.h"
#include "tt.h"
#include "threads.h"
#include "timeman.h"
#include "perft.h"
#include "movegen.h"
#include "tune.h"

#ifndef VERSION
#define VERSION "(Unknown)"
#endif

namespace UCI {
  Position main_pos;

  Move parse_move(const std::string &move_str) {
    MoveList list;
    Movegen::generate_pseudo_moves(main_pos, list);
    for (int i = 0; i < list.count; i++) {
      if (move_to_string(list.moves[i]) == move_str) return list.moves[i];
    }
    return 0;
  }

  void print_board() {
    std::cout << "\n +---+---+---+---+---+---+---+---+\n";
    for (int r = 7; r >= 0; r--) {
      std::cout << " |";
      for (int f = 0; f < 8; f++) {
        int sq = r * 8 + f;
        Piece p = main_pos.board_sq[sq];
        if (p == NONE) {
          std::cout << "   |";
        } else {
          bool is_white = (main_pos.piece_bb[WHITE][p] & (1ULL << sq));
          char c = "?pnbrqk"[p + 1];
          if (is_white) c = toupper(c);
          std::cout << " " << c << " |";
        }
      }
      std::cout << " " << r + 1 << "\n +---+---+---+---+---+---+---+---+\n";
    }
    std::cout << "   a   b   c   d   e   f   g   h\n\n";
    std::cout << "Fen: " << main_pos.get_fen() << "\n";
    std::cout << "Key: " << std::hex << std::uppercase << main_pos.hash_key << std::dec << "\n\n";
  }

  bool execute_command(const std::string& line) {
    if (line.empty()) return true;
        
    std::istringstream iss(line);
    std::string token;
    iss >> token;

    if (token == "quit") return false;

    else if (token == "d") {
      print_board();
    }
    else if (token == "eval") {
      // NNUE::Accumulator acc;
      // if (NNUE::is_loaded) {
      //   NNUE::refresh_accumulator(main_pos, acc);
      // }
      int score = Eval::evaluate(main_pos/*, acc*/);
      std::cout << "Static Eval: " << score << " cp" << std::endl;
    }
    else if (token == "uci") {
      std::cout << "id name Thorn " << VERSION << std::endl;
      std::cout << "option name Hash type spin default 16 min 1 max 1024" << std::endl;
      std::cout << "option name Threads type spin default 1 min 1 max 128" << std::endl;
      std::cout << "uciok" << std::endl;
    }
    else if (token == "isready") std::cout << "readyok" << std::endl;
    else if (token == "ucinewgame") {
      main_pos.parse_fen(START_FEN);
      TT::clear();
    }
    else if (token == "setoption") {
      std::string name, val_str, temp;
      iss >> temp; // Skip 'name'
      while (iss >> temp && temp != "value") name += (name.empty() ? "" : " ") + temp;
      while (iss >> temp) val_str += (val_str.empty() ? "" : " ") + temp;

      if (name == "Hash") { try { TT::resize(std::stoi(val_str)); } catch (...) {} }
      else if (name == "Threads") { try { Threads::GLOBAL_THREADS = std::max(1, std::stoi(val_str)); } catch (...) {} }
    }
    else if (token == "position") {
      iss >> token;
      if (token == "startpos") {
        main_pos.parse_fen(START_FEN);
        iss >> token;
      } else if (token == "fen") {
        std::string fen = "";
        for (int i = 0; i < 6; i++) { if (iss >> token) fen += token + " "; }
        main_pos.parse_fen(fen);
        iss >> token;
      }
      if (token == "moves") {
        while (iss >> token) {
          Move m = parse_move(token);
          if (m) main_pos.make_move(m);
        }
      }
    }
    else if (token == "bench") {
      int tt_size = 16, threads = 1, limit = 5;
      std::string fen_file = "default";
      std::string limit_type = "depth";

      std::vector<std::string> params;
      std::string p;
      while (iss >> p) params.push_back(p);
      
      if (params.size() > 0) try { tt_size = std::stoi(params[0]); } catch (...) {}
      if (params.size() > 1) try { threads = std::stoi(params[1]); } catch (...) {}
      if (params.size() > 2) try { limit = std::stoi(params[2]); } catch (...) {}
      if (params.size() > 3) fen_file = params[3];
      if (params.size() > 4) limit_type = params[4];

      TT::resize(tt_size);
      Threads::GLOBAL_THREADS = std::max(1, threads);

      std::vector<std::string> bench_fens;

      if (fen_file == "default") {
        bench_fens = {
          START_FEN,
          "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
          "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
          "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
          "rnbqkbnr/p1pppppp/8/8/1pP5/8/PP1PPPPP/RNBQKBNR w KQkq - 0 3",

          "rnbqnrk1/ppp3bp/3p2p1/3Ppp2/2P1P3/2N1BP2/PP1Q2PP/R3KBNR w KQ f6 0 9",
          "r1bq1rk1/1pp2ppp/2n1pn2/p2p2B1/2PP4/P1Q2N2/1P2PPPP/R3KB1R w KQ a6 0 9",
          "rn1q1rk1/pbp1bppp/1p3n2/3p4/3PP3/2NB1N2/PP3PPP/R1BQK2R w KQ - 0 9",
          "r2qkbnr/pp1npppb/2p5/7p/3P1N1P/6N1/PPP2PP1/R1BQKB1R w KQkq - 4 9",
          "rnbqnrk1/pp2bp1p/3p2p1/2pPp3/2P1P3/2N3P1/PP2NPBP/R1BQK2R w KQ - 0 9",
          "r1b1k2r/pppn1pbp/3p2p1/4p2n/2PPP2q/2NBBP2/PP2N1PP/R2QK2R w KQkq - 3 9",
          "rn2k2r/ppqbnppp/4p3/2ppP3/P2P4/2P2N2/2P2PPP/R1BQKB1R w KQkq - 1 9",
          "r1bqnrk1/pp1nbppp/3p4/2pPp3/2P1P3/2N2NP1/PP3PBP/R1BQK2R w KQ - 5 9",
          "r2qk1nr/1ppb2bp/p1np1pp1/4p3/B2PP3/2P2N2/PP3PPP/RNBQR1K1 w kq - 0 9",
          "r1bqk2r/pp1n1ppp/4pn2/2b5/3P4/3B1N2/PPP2PPP/R1BQ1RK1 w kq - 0 9",
          "rnbqkb1r/ppp1pppp/8/8/4P3/2N2N2/PP1P1PPP/R1BQK2R w KQkq - 4 9",
          "r1b1k2r/pp1n1ppp/2p1p3/q2p4/1bPPnB2/2N1P3/PPQN1PPP/R3KB1R w KQkq - 5 9",
          "r1bqnrk1/pp1nbppp/3p4/2pPp3/2P1P3/2N3P1/PP2NPBP/R1BQK2R w KQ - 5 9",
          "rn3rk1/ppq1ppbp/2pp1np1/8/2PPP1b1/2N2NP1/PP3PBP/R1BQ1RK1 w - - 1 9",
          "rn1q1rk1/ppp2pp1/3p1n1p/4p3/1bPP2bB/2N1P3/PPQ1NPPP/R3KB1R w KQ - 2 9",
          "rn1qkb1r/p4ppp/1pp1pn2/3p3b/2PP3N/1QN1P2P/PP3PP1/R1B1KB1R w KQkq - 1 9",
          "r2qk2r/ppp1ppbp/3p1np1/3Pn3/2P1P3/2N2B2/PP3PPP/R1BQK2R w KQkq - 1 9",
          "r1bqkb1r/ppp1p1pp/1n1pp3/6N1/2PP4/3n4/PP3PPP/RNBQK2R w KQkq - 0 9",
          "r2q1rk1/ppp2ppp/2np1n2/2b1pb2/2P5/2N1P1PP/PP1PNPB1/R1BQ1RK1 w - - 1 9",
          "r2qk1nr/1ppb2bp/p1np1pp1/4p1B1/B1PPP3/2N2N2/PP3PPP/R2QK2R w KQkq - 0 9",
          "r2qkb1r/pbp2pp1/1pn1p2p/3n4/3P4/P1NB1N2/1PP1QPPP/R1B1K2R w KQkq - 0 9",
          "r1b1k2r/ppqnbppp/2pp4/4p1Pn/3PP3/2N1BP2/PPPQ3P/R3KBNR w KQkq - 1 9",
          "r2q1rk1/p1pp1pbp/np2pnp1/8/3P1B2/2P1PN1P/PP1N1PP1/R2QK2R w KQ - 1 9",
          "8/pp2nkR1/5n1p/3p4/5p2/P2BP3/1PPKN3/8 b - - 0 31",
          "3n4/2k3p1/p4r2/1pp4P/5PB1/P6P/1KP5/5R2 w - - 0 32",
          "4r1k1/5p1p/6pP/2b5/1p3R2/pP2BKP1/P4P2/8 b - - 0 38",
          "1b6/4k1p1/3p3p/p6P/Bp6/2r1P1P1/4KP2/R7 w - - 0 70",
          "4k3/2Rb4/3r3p/4p1p1/5p2/7P/4R1P1/6K1 w - - 0 61",

          "4rrk1/pp1n3p/3q2pQ/2p1pb2/2PP4/2P3N1/P2B2PP/4RRK1 b - - 7 19",
          "rq3rk1/ppp2ppp/1bnpb3/3N2B1/3NP3/7P/PPPQ1PP1/2KR3R w - - 7 14",
          "r1bq1r1k/1pp1n1pp/1p1p4/4p2Q/4Pp2/1BNP4/PPP2PPP/3R1RK1 w - - 2 14",
          "r3r1k1/2p2ppp/p1p1bn2/8/1q2P3/2NPQN2/PPP3PP/R4RK1 b - - 2 15",
          "r1bbk1nr/pp3p1p/2n5/1N4p1/2Np1B2/8/PPP2PPP/2KR1B1R w kq - 0 13",
          "r1bq1rk1/ppp1nppp/4n3/3p3Q/3P4/1BP1B3/PP1N2PP/R4RK1 w - - 1 16",
          "4r1k1/r1q2ppp/ppp2n2/4P3/5Rb1/1N1BQ3/PPP3PP/R5K1 w - - 1 17",
          "2rqkb1r/ppp2p2/2npb1p1/1N1Nn2p/2P1PP2/8/PP2B1PP/R1BQK2R b KQ - 0 11",
          "r1bq1r1k/b1p1npp1/p2p3p/1p6/3PP3/1B2NN2/PP3PPP/R2Q1RK1 w - - 1 16",
          "3r1rk1/p5pp/bpp1pp2/8/q1PP1P2/b3P3/P2NQRPP/1R2B1K1 b - - 6 22",
          "r1q2rk1/2p1bppp/2Pp4/p6b/Q1PNp3/4B3/PP1R1PPP/2K4R w - - 2 18",
          "4k2r/1pb2ppp/1p2p3/1R1p4/3P4/2r1PN2/P4PPP/1R4K1 b - - 3 22",
          "3q2k1/pb3p1p/4pbp1/2r5/PpN2N2/1P2P2P/5PP1/Q2R2K1 b - - 4 26",
          "6k1/6p1/6Pp/ppp5/3pn2P/1P3K2/1PP2P2/3N4 b - - 0 1",
          "3b4/5kp1/1p1p1p1p/pP1PpP1P/P1P1P3/3KN3/8/8 w - - 0 1",
          "2K5/p7/7P/5pR1/8/5k2/r7/8 w - - 0 1",
          "8/6pk/1p6/8/PP3p1p/5P2/4KP1q/3Q4 w - - 0 1",
          "7k/3p2pp/4q3/8/4Q3/5Kp1/P6b/8 w - - 0 1",
          "8/2p5/8/2kPKp1p/2p4P/2P5/3P4/8 w - - 0 1",
          "8/1p3pp1/7p/5P1P/2k3P1/8/2K2P2/8 w - - 0 1",
          "8/pp2r1k1/2p1p3/3pP2p/1P1P1P1P/P5KR/8/8 w - - 0 1",
          "8/3p4/p1bk3p/Pp6/1Kp1PpPp/2P2P1P/2P5/5B2 b - - 0 1",
          "5k2/7R/4P2p/5K2/p1r2P1p/8/8/8 b - - 0 1",
          "6k1/6p1/P6p/r1N5/5p2/7P/1b3PP1/4R1K1 w - - 0 1",
          "1r3k2/4q3/2Pp3b/3Bp3/2Q2p2/1p1P2P1/1P2KP2/3N4 w - - 0 1",
          "6k1/4pp1p/3p2p1/P1pPb3/R7/1r2P1PP/3B1P2/6K1 w - - 0 1",
          "8/3p3B/5p2/5P2/p7/PP5b/k7/6K1 w - - 0 1",
          "5rk1/q6p/2p3bR/1pPp1rP1/1P1Pp3/P3B1Q1/1K3P2/R7 w - - 93 90",
          "4rrk1/1p1nq3/p7/2p1P1pp/3P2bp/3Q1Bn1/PPPB4/1K2R1NR w - - 40 21",
          "r3k2r/3nnpbp/q2pp1p1/p7/Pp1PPPP1/4BNN1/1P5P/R2Q1RK1 w kq - 0 16",
          "3Qb1k1/1r2ppb1/pN1n2q1/Pp1Pp1Pr/4P2p/4BP2/4B1R1/1R5K b - - 11 40",
          "8/8/8/8/5kp1/P7/8/1K1N4 w - - 0 1",
          "8/8/8/5N2/8/p7/8/2NK3k w - - 0 1",
          "8/3k4/8/8/8/4B3/4KB2/2B5 w - - 0 1",
          "8/8/1P6/5pr1/8/4R3/7k/2K5 w - - 0 1",
          "8/2p4P/8/kr6/6R1/8/8/1K6 w - - 0 1",
          "8/8/3P3k/8/1p6/8/1P6/1K3n2 b - - 0 1",
          "8/R7/2q5/8/6k1/8/1P5p/K6R w - - 0 124",
          "6k1/3b3r/1p1p4/p1n2p2/1PPNpP1q/P3Q1p1/1R1RB1P1/5K2 b - - 0 1",
          "r2r1n2/pp2bk2/2p1p2p/3q4/3PN1QP/2P3R1/P4PP1/5RK1 w - - 0 1",
          "8/8/8/8/8/6k1/6p1/6K1 w - - 0 1",
          "7k/7P/6K1/8/3B4/8/8/8 b - - 0 1"
        };
      }
      else if (fen_file == "current") {
        bench_fens.push_back(main_pos.get_fen());
      }
      else {
        std::ifstream file(fen_file);
        if (file.is_open()) {
          std::string f_line;
          while (std::getline(file, f_line)) {
              if (!f_line.empty()) bench_fens.push_back(f_line);
          }
          file.close();
        } else {
          std::cout << "Error: Could not open FEN file: " << fen_file << std::endl;
          return true;
        }
      }

      if (bench_fens.empty()) {
        std::cout << "Error: No FENs to bench!" << std::endl;
        return true;
      }

      uint64_t total_nodes = 0;
      auto bench_start = std::chrono::high_resolution_clock::now();

      for (size_t i = 0; i < bench_fens.size(); i++) {
        main_pos.parse_fen(bench_fens[i]);
        std::cout << "\nPosition: " << (i + 1) << "/" << bench_fens.size() 
                  << " (" << bench_fens[i] << ")" << std::endl;

        if (limit_type == "perft") {
          total_nodes += Perft::divide(main_pos, limit);
        } else {
          // Default to standard search
          Threads::start_search(main_pos, limit, -1);
          total_nodes += Threads::global_nodes.load(std::memory_order_relaxed);
        }
      }

      auto bench_end = std::chrono::high_resolution_clock::now();
      int64_t elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(bench_end - bench_start).count();
      if (elapsed == 0) elapsed = 1; // Prevent division by zero

      uint64_t nps = (total_nodes * 1000) / elapsed;

      std::cout << "\n===========================" << std::endl;
      std::cout << "Total time (ms) : " << elapsed << std::endl;
      std::cout << "Nodes searched  : " << total_nodes << std::endl;
      std::cout << "Nodes/second    : " << nps << std::endl;
    }
    else if (token == "tune") {
      std::string filepath;
      if (iss >> filepath) {
        Tune::run_tuner(filepath);
      } else {
        std::cout << "Usage: tune <filepath.epd>" << std::endl;
      }
    }
    else if (token == "go") {
      int depth = 64, wtime = -1, btime = -1, winc = 0, binc = 0, movetime = -1, movestogo = 0;
      bool is_perft = false;
      int perft_depth = 5;

      while (iss >> token) {
        if (token == "perft") { is_perft = true; if (!(iss >> perft_depth)) perft_depth = 5; break; }
        else if (token == "depth") iss >> depth;
        else if (token == "wtime") iss >> wtime;
        else if (token == "btime") iss >> btime;
        else if (token == "winc") iss >> winc;
        else if (token == "binc") iss >> binc;
        else if (token == "movetime") iss >> movetime;
        else if (token == "movestogo") iss >> movestogo;
      }

      if (is_perft) {
        Perft::divide(main_pos, perft_depth);
        return true;
      }

      int allocated_time = TimeMan::calculate_time(main_pos, wtime, btime, winc, binc, movetime, movestogo);
      Threads::start_search(main_pos, depth, allocated_time);
    }
    else {
      std::cout << "Unknown command: '" << token << "'" << std::endl;
    }
    return true;
  }

  /// loop() initiates the interactive command-line loop. It blocks and waits 
  /// for GUI/user input until 'quit' is typed.

  void loop(int argc, char* argv[]) {
    std::string cmd = "";
    for (int i = 1; i < argc; ++i) {
      cmd += std::string(argv[i]) + " ";
    }

    main_pos.parse_fen(START_FEN);

    if (!cmd.empty()) {
      execute_command(cmd);
      return;
    }

    std::cout << "Thorn " << VERSION << " by D. Sipek" << std::endl;
    std::string line;

    while (std::getline(std::cin, line)) {
      if (!execute_command(line)) {
        break;
      }
    }
  }
}
