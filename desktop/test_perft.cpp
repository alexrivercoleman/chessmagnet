// Checks chess-library's move generator with perft: count every legal move
// sequence to a fixed depth and compare against well-known totals.
// If move generation had a bug (castling, en passant, pins, promotion...),
// these numbers would be off.
#include <chrono>
#include <cstdio>

#include "chess_lib.h"

using namespace chess;

static uint64_t perft(Board& board, int depth) {
  Movelist moves;
  movegen::legalmoves(moves, board);
  if (depth == 1) return moves.size();
  uint64_t nodes = 0;
  for (const Move& m : moves) {
    board.makeMove(m);
    nodes += perft(board, depth - 1);
    board.unmakeMove(m);
  }
  return nodes;
}

struct Case {
  const char* name;
  const char* fen;
  int depth;
  uint64_t expected;
};

int main() {
  const Case cases[] = {
      // Required: starting position, depths 1-4.
      {"start", constants::STARTPOS, 1, 20},
      {"start", constants::STARTPOS, 2, 400},
      {"start", constants::STARTPOS, 3, 8902},
      {"start", constants::STARTPOS, 4, 197281},
      // Extra: "Kiwipete", a position full of castling/en passant/promotion
      // edge cases (values from the Chess Programming Wiki).
      {"kiwipete", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 1, 48},
      {"kiwipete", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 2, 2039},
      {"kiwipete", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 3, 97862},
  };
  int failures = 0;
  for (const Case& c : cases) {
    Board board(c.fen);
    const auto t0 = std::chrono::steady_clock::now();
    const uint64_t got = perft(board, c.depth);
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    const bool ok = got == c.expected;
    failures += ok ? 0 : 1;
    std::printf("%-4s %-9s depth %d: %8llu (expected %8llu)  %7.1f ms\n", ok ? "PASS" : "FAIL", c.name, c.depth,
                static_cast<unsigned long long>(got), static_cast<unsigned long long>(c.expected), ms);
  }
  std::printf(failures ? "\n%d perft check(s) FAILED\n" : "\nAll perft checks passed.\n", failures);
  return failures ? 1 : 0;
}
