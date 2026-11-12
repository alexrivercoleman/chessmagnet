// Game: the rules side of the app. Thin wrapper around chess-library that
// speaks in plain square numbers (a1 = 0, b1 = 1, ... h8 = 63) and adds
// last-move tracking plus save/restore.
#pragma once
#include <stdint.h>

#include <string>
#include <vector>

#include "chess_lib.h"

namespace cm {

enum class Outcome : uint8_t {
  Ongoing,
  Checkmate,
  Stalemate,
  InsufficientMaterial,
  FiftyMoveRule,
  ThreefoldRepetition,
};

struct MoveResult {
  bool capture = false;    // includes en passant
  bool castle = false;
  bool promotion = false;
  bool givesCheck = false;
};

class Game {
 public:
  Game() { reset(); }

  void reset();  // standard starting position

  // -1 for an empty square, otherwise a chess::Piece value
  // (0..5 = white P N B R Q K, 6..11 = black P N B R Q K).
  int pieceAt(int sq) const;
  bool whiteToMove() const;

  // Bit i set = a legal destination for the piece on `from`.
  // For castling the destination is the king's target square (g1/c1/...).
  uint64_t targetsFrom(int from) const;
  bool needsPromotion(int from, int to) const;

  // Plays a legal move. `promo` is 'q', 'r', 'b' or 'n' (ignored otherwise).
  // Returns false (and changes nothing) if the move is not legal.
  bool play(int from, int to, char promo = 'q', MoveResult* result = nullptr);

  int lastFrom() const { return lastFrom_; }
  int lastTo() const { return lastTo_; }
  int checkedKing() const;  // square of the side-to-move's king if in check, else -1

  Outcome outcome() const { return outcome_; }
  // Only meaningful when outcome() == Checkmate.
  bool whiteWon() const { return outcome_ == Outcome::Checkmate && !whiteToMove(); }

  std::string fen() const { return board_.getFen(); }

  // Persistence. The saved text is:
  //   line 1: "chessmagnet 1"
  //   line 2: current position as FEN
  //   line 3: FEN of the position before the last irreversible move
  //   line 4: moves (UCI, space separated) from line 3 to line 2
  // Line 2 alone restores the position; lines 3-4 also restore what's
  // needed for threefold-repetition detection and the last-move highlight.
  std::string serialize() const;
  bool restore(const std::string& saved);
  bool setFen(const std::string& fen);  // for tests and previews

 private:
  void refresh();  // recompute cached legal moves + outcome
  int uiTarget(const chess::Move& m) const;

  chess::Board board_;
  chess::Movelist legal_;
  Outcome outcome_ = Outcome::Ongoing;
  int lastFrom_ = -1, lastTo_ = -1;
  std::string anchorFen_;
  std::vector<std::string> history_;
};

}  // namespace cm
