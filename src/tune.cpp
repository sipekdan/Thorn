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
#include <fstream>
#include <vector>
#include <cmath>
#include <iomanip>
#include <thread>

#include "tune.h"
#include "evaluate.h"
#include "position.h"

namespace Tune {

  struct TunerEntry {
    Position pos;
    double result; // 1.0 (White wins), 0.5 (Draw), 0.0 (Black wins)
  };

  std::vector<TunerEntry> dataset;
  
  /// K is the sigmoid scaling constant.
  const double K = 400.0; 

  /// sigmoid() converts a raw centipawn score into an Expected Win Percentage (0.0 to 1.0).

  double sigmoid(int score) {
    return 1.0 / (1.0 + std::pow(10.0, -score / K));
  }

  /// wdl_to_cp() reverses the sigmoid math to turn a WDL probability back into centipawns.
  double wdl_to_cp(double wdl) {
    // We clamp the probability slightly above 0.0 and below 1.0. 
    // This prevents division by zero or calculating log(0) for forced mate positions.
    wdl = std::max(0.0001, std::min(0.9999, wdl));
    return -400.0 * std::log10((1.0 - wdl) / wdl);
  }

  /// calculate_mse() calculates the Mean Squared Error across the entire dataset.
  /// The goal of the tuner is to make this number as small as possible.
  
//   double calculate_mse() {
//     double total_error = 0.0;
    
//     for (const auto& entry : dataset) {
//       int score = Eval::evaluate(entry.pos);
      
//       // The evaluation function is always relative to the side to move. 
//       // We must normalize it to White's perspective for the sigmoid.
//       if (entry.pos.side_to_move == BLACK) {
//         score = -score; 
//       }

//       double expected = sigmoid(score);
//       double error = entry.result - expected;
//       total_error += error * error;
//     }
    
//     return total_error / dataset.size();
//   }

  double calculate_mse() {
    int num_threads = std::thread::hardware_concurrency();
    if (num_threads == 0) num_threads = 4; // Fallback

    std::vector<double> thread_errors(num_threads, 0.0);
    std::vector<std::thread> threads;

    size_t chunk_size = dataset.size() / num_threads;

    for (int t = 0; t < num_threads; ++t) {
      threads.emplace_back([&, t]() {
        size_t start = t * chunk_size;
        size_t end = (t == num_threads - 1) ? dataset.size() : (start + chunk_size);
        double local_error = 0.0;

        for (size_t i = start; i < end; ++i) {
          const auto& entry = dataset[i];
          int score = Eval::evaluate(entry.pos);
          
          if (entry.pos.side_to_move == BLACK) {
            score = -score; 
          }

          double expected = sigmoid(score);
          double error = entry.result - expected;
          local_error += error * error;
        }
        thread_errors[t] = local_error;
      });
    }

    double total_error = 0.0;
    for (int t = 0; t < num_threads; ++t) {
      threads[t].join();
      total_error += thread_errors[t];
    }

    return total_error / dataset.size();
  }

  /// calculate_mae() calculates the Mean Absolute Error in pure Centipawns.
  double calculate_mae() {
    int num_threads = std::thread::hardware_concurrency();
    if (num_threads == 0) num_threads = 4;

    std::vector<double> thread_errors(num_threads, 0.0);
    std::vector<std::thread> threads;

    size_t chunk_size = dataset.size() / num_threads;

    for (int t = 0; t < num_threads; ++t) {
      threads.emplace_back([&, t]() {
        size_t start = t * chunk_size;
        size_t end = (t == num_threads - 1) ? dataset.size() : (start + chunk_size);
        double local_error = 0.0;

        for (size_t i = start; i < end; ++i) {
          const auto& entry = dataset[i];
          int score = Eval::evaluate(entry.pos);
          
          if (entry.pos.side_to_move == BLACK) {
            score = -score; 
          }

          // Convert the WDL probability back into Stockfish's Centipawns
          double stockfish_cp = wdl_to_cp(entry.result);
          
          // Calculate how far off our engine is in raw centipawns
          double error = std::abs(stockfish_cp - (double)score);
          local_error += error;
        }
        thread_errors[t] = local_error;
      });
    }

    double total_error = 0.0;
    for (int t = 0; t < num_threads; ++t) {
      threads[t].join();
      total_error += thread_errors[t];
    }

    return total_error / dataset.size();
  }

  /// tune_parameter() applies Coordinate Descent. It tests +1 and -1 on a specific 
  /// evaluation weight to see which direction mathematically reduces the MSE.
  
  void tune_parameter(int& param, double& current_mse, int step) {
    int best_val = param;
    double best_mse = current_mse;

    // Test incrementing the weight
    param += step;
    double mse_up = calculate_mse();
    if (mse_up < best_mse) {
      best_mse = mse_up;
      best_val = param;
    }

    // Test decrementing the weight (subtract 2*step to counteract the +step above)
    param -= (step * 2); 
    double mse_down = calculate_mse();
    if (mse_down < best_mse) {
      best_mse = mse_down;
      best_val = param;
    }

    // Lock in the best found value and update the baseline MSE
    param = best_val;
    current_mse = best_mse;
  }

  /// print_table() outputs the tuned array so you can copy and paste it 
  /// directly back into evaluate.cpp.

  void print_table(const std::string& name, const Eval::Score table[64]) {
    std::cout << "  Score " << name << "[64] = {\n";
    for (int r = 0; r <= 7; r++) {
      std::cout << "    ";
      for (int f = 0; f < 8; f++) {
        int sq = r * 8 + f;
        std::cout << "S(" << std::setw(4) << table[sq].mg << ", " << std::setw(4) << table[sq].eg << ")";
        if (sq != 7) std::cout << ", ";
      }
      std::cout << "\n";
    }
    std::cout << "  };\n\n";
  }

  void print_array_8(const std::string& name, const Eval::Score table[8]) {
    std::cout << "  Score " << name << "[8] = { ";
    for (int i = 0; i < 8; i++) {
       std::cout << "S(" << table[i].mg << ", " << table[i].eg << ")";
       if (i != 7) std::cout << ", ";
    }
    std::cout << " };\n\n";
  }

  /// run_tuner() loads the FEN dataset, iteratively tunes the piece-square tables, 
  /// and prints the optimized result.

  void run_tuner(const std::string& filepath) {
    std::cout << "Loading dataset from " << filepath << "..." << std::endl;
    
    std::ifstream file(filepath);
    std::string line;
    
    // Parse FENs and results. Format expected: "FEN [1.0]"
    while (std::getline(file, line)) {
      if (line.empty()) continue;
      
      size_t bracket_pos = line.find('[');
      if (bracket_pos != std::string::npos) {
        std::string fen = line.substr(0, bracket_pos - 1);
        std::string res_str = line.substr(bracket_pos + 1, line.length() - bracket_pos - 2);
        
        TunerEntry entry;
        entry.pos.parse_fen(fen);
        entry.result = std::stod(res_str);
        dataset.push_back(entry);
      }
    }

    std::cout << "Loaded " << dataset.size() << " positions." << std::endl;

    double current_mse = calculate_mse();
    double current_mae = calculate_mae();

    std::cout << "Initial MSE: " << std::fixed << std::setprecision(6) << current_mse 
              << " (Avg Error: " << std::setprecision(1) << current_mae << " cp)\n";

    // Define how many full sweeps over the tables you want to perform
    int iterations = 10; 
    
    for (int iter = 1; iter <= iterations; iter++) {
      std::cout << "Iteration " << iter << "..." << std::endl;

      int step = std::max(1, 8 / iter);

      for (int p = 0; p < 5; p++) {
        tune_parameter(Eval::piece_value[p].mg, current_mse, step);
        tune_parameter(Eval::piece_value[p].eg, current_mse, step);
      }

      for (int i = 0; i < 64; i++) {
        if (i >= 8 && i < 56) {
          tune_parameter(Eval::pawn_table[i].mg, current_mse, step);
          tune_parameter(Eval::pawn_table[i].eg, current_mse, step);
        }
        
        tune_parameter(Eval::knight_table[i].mg, current_mse, step);
        tune_parameter(Eval::knight_table[i].eg, current_mse, step);

        tune_parameter(Eval::bishop_table[i].mg, current_mse, step);
        tune_parameter(Eval::bishop_table[i].eg, current_mse, step);

        tune_parameter(Eval::rook_table[i].mg, current_mse, step);
        tune_parameter(Eval::rook_table[i].eg, current_mse, step);

        tune_parameter(Eval::queen_table[i].mg, current_mse, step);
        tune_parameter(Eval::queen_table[i].eg, current_mse, step);

        tune_parameter(Eval::king_table[i].mg, current_mse, step);
        tune_parameter(Eval::king_table[i].eg, current_mse, step);
      }

      for (int i = 1; i < 7; i++) {
        tune_parameter(Eval::passed_pawn_bonus[i].mg, current_mse, step);
        tune_parameter(Eval::passed_pawn_bonus[i].eg, current_mse, step);
      }

      tune_parameter(Eval::BISHOP_PAIR.mg, current_mse, step);
      tune_parameter(Eval::BISHOP_PAIR.eg, current_mse, step);
      
      tune_parameter(Eval::ROOK_SEMI_OPEN.mg, current_mse, step);
      tune_parameter(Eval::ROOK_SEMI_OPEN.eg, current_mse, step);
      
      tune_parameter(Eval::ROOK_OPEN.mg, current_mse, step);
      tune_parameter(Eval::ROOK_OPEN.eg, current_mse, step);
      
      tune_parameter(Eval::TEMPO_BONUS, current_mse, step);

      std::cout << "MSE after iteration " << iter << " (step " << step << "): " 
                << std::fixed << std::setprecision(6) << current_mse 
                << " (Avg Error: " << std::setprecision(1) << calculate_mae() << " cp)\n";
    }

    std::cout << "\n=== Tuning Complete! Copy the tables below ===\n\n";

    std::cout << "  Score piece_value[6] = { ";
    for (int p = 0; p < 6; p++) {
      std::cout << "S(" << Eval::piece_value[p].mg << ", " << Eval::piece_value[p].eg << ")";
      if (p != 5) std::cout << ", ";
    }
    std::cout << " };\n\n";

    std::cout << "  Score BISHOP_PAIR = S(" << Eval::BISHOP_PAIR.mg << ", " << Eval::BISHOP_PAIR.eg << ");\n";
    std::cout << "  Score ROOK_SEMI_OPEN = S(" << Eval::ROOK_SEMI_OPEN.mg << ", " << Eval::ROOK_SEMI_OPEN.eg << ");\n";
    std::cout << "  Score ROOK_OPEN = S(" << Eval::ROOK_OPEN.mg << ", " << Eval::ROOK_OPEN.eg << ");\n";
    std::cout << "  int TEMPO_BONUS = " << Eval::TEMPO_BONUS << ";\n\n";

    print_array_8("passed_pawn_bonus", Eval::passed_pawn_bonus);

    print_table("pawn_table", Eval::pawn_table);
    print_table("knight_table", Eval::knight_table);
    print_table("bishop_table", Eval::bishop_table);
    print_table("rook_table", Eval::rook_table);
    print_table("queen_table", Eval::queen_table);
    print_table("king_table", Eval::king_table);
  }
}
