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

#include "bitboard.h"
#include "zobrist.h"
#include "evaluate.h"
#include "nnue.h"
#include "tt.h"
#include "uci.h"
#include "search.h"

int main(int argc, char* argv[]) {
  Bitboard::init();
  Zobrist::init();
  Eval::init();
  Search::init();
  TT::resize(16);

  // Bitboard::print(Bitboard::knight_attacks[SQ_A6]);
  // Bitboard::print(Bitboard::king_attacks[SQ_E4]);
  // Bitboard::print(Bitboard::pawn_attacks[BLACK][SQ_E7]);
  // Bitboard::print(Bitboard::between_bb[SQ_A7][SQ_G6]);

  // const char *nnue_file = "nn-a7f892f7c4b2.nnue";
  // if (NNUE::load_network(nnue_file)) {
  //   std::cout << "info string NNUE " << nnue_file << " loaded successfully" << std::endl;
  // } else {
  //   std::cout << "info string NNUE " << nnue_file << " network file not found" << std::endl;
  // }

  UCI::loop(argc, argv);

  return 0;
}