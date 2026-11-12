#include "game.h"

namespace cm {

using chess::Board;
using chess::Move;
using chess::Movelist;
using chess::Piece;
using chess::PieceType;
using chess::Square;

void Game::reset() {
  board_ = Board(chess::constants::STARTPOS);
  anchorFen_ = board_.getFen();
  history_.clear();
  lastFrom_ = lastTo_ = -1;
  refresh();
}

void Game::refresh() {
  legal_.clear();
  chess::movegen::legalmoves(legal_, board_);
  switch (board_.isGameOver().first) {
    case chess::GameResultReason::CHECKMATE: outcome_ = Outcome::Checkmate; break;
    case chess::GameResultReason::STALEMATE: outcome_ = Outcome::Stalemate; break;
    case chess::GameResultReason::INSUFFICIENT_MATERIAL: outcome_ = Outcome::InsufficientMaterial; break;
    case chess::GameResultReason::FIFTY_MOVE_RULE: outcome_ = Outcome::FiftyMoveRule; break;
    case chess::GameResultReason::THREEFOLD_REPETITION: outcome_ = Outcome::ThreefoldRepetition; break;
    default: outcome_ = Outcome::Ongoing; break;
  }
}

int Game::pieceAt(int sq) const {
  const Piece p = board_.at(Square(sq));
  return p == Piece::NONE ? -1 : static_cast<int>(p);
}

bool Game::whiteToMove() const { return board_.sideToMove() == chess::Color::WHITE; }

int Game::uiTarget(const Move& m) const {
  if (m.typeOf() != Move::CASTLING) return m.to().index();
  // chess-library encodes castling as "king takes own rook"; the player taps
  // the square the king lands on instead (g-file or c-file).
  const int from = m.from().index(), rookSq = m.to().index();
  const int rank = from / 8;
  return rank * 8 + ((rookSq % 8) > (from % 8) ? 6 : 2);
}

uint64_t Game::targetsFrom(int from) const {
  if (outcome_ != Outcome::Ongoing) return 0;
  uint64_t mask = 0;
  for (const Move& m : legal_) {
    if (m.from().index() == from) mask |= 1ull << uiTarget(m);
  }
  return mask;
}

bool Game::needsPromotion(int from, int to) const {
  for (const Move& m : legal_) {
    if (m.from().index() == from && uiTarget(m) == to && m.typeOf() == Move::PROMOTION) return true;
  }
  return false;
}

bool Game::play(int from, int to, char promo, MoveResult* result) {
  if (outcome_ != Outcome::Ongoing) return false;
  PieceType want = PieceType::QUEEN;
  switch (promo) {
    case 'r': want = PieceType::ROOK; break;
    case 'b': want = PieceType::BISHOP; break;
    case 'n': want = PieceType::KNIGHT; break;
    default: break;
  }
  for (const Move& m : legal_) {
    if (m.from().index() != from || uiTarget(m) != to) continue;
    if (m.typeOf() == Move::PROMOTION && m.promotionType() != want) continue;

    MoveResult r;
    r.castle = m.typeOf() == Move::CASTLING;
    r.promotion = m.typeOf() == Move::PROMOTION;
    r.capture = !r.castle && (board_.at(m.to()) != Piece::NONE || m.typeOf() == Move::ENPASSANT);
    r.givesCheck = board_.givesCheck(m) != chess::CheckType::NO_CHECK;
    const bool pawnMove = board_.at(m.from()).type() == PieceType::PAWN;

    // Positions before a capture or pawn move can never repeat, so we only
    // need to remember history from just before the latest such move.
    if (r.capture || pawnMove) {
      anchorFen_ = board_.getFen();
      history_.clear();
    }
    history_.push_back(chess::uci::moveToUci(m));
    board_.makeMove(m);
    lastFrom_ = from;
    lastTo_ = to;
    refresh();
    if (result) *result = r;
    return true;
  }
  return false;
}

int Game::checkedKing() const {
  if (!board_.inCheck()) return -1;
  return board_.kingSq(board_.sideToMove()).index();
}

std::string Game::serialize() const {
  std::string s = "chessmagnet 1\n" + board_.getFen() + "\n" + anchorFen_ + "\n";
  for (size_t i = 0; i < history_.size(); ++i) {
    if (i) s += ' ';
    s += history_[i];
  }
  s += '\n';
  return s;
}

namespace {
// A FEN must parse and have exactly one king per side to be playable.
bool loadFen(Board& b, const std::string& fen) {
  if (!b.setFen(fen)) return false;
  return b.pieces(PieceType::KING, chess::Color::WHITE).count() == 1 &&
         b.pieces(PieceType::KING, chess::Color::BLACK).count() == 1;
}

std::vector<std::string> splitLines(const std::string& s) {
  std::vector<std::string> out;
  size_t start = 0;
  while (start <= s.size()) {
    size_t nl = s.find('\n', start);
    if (nl == std::string::npos) nl = s.size();
    out.push_back(s.substr(start, nl - start));
    start = nl + 1;
  }
  return out;
}
}  // namespace

bool Game::setFen(const std::string& fen) {
  Board b;
  if (!loadFen(b, fen)) return false;
  board_ = b;
  anchorFen_ = board_.getFen();
  history_.clear();
  lastFrom_ = lastTo_ = -1;
  refresh();
  return true;
}

bool Game::restore(const std::string& saved) {
  const std::vector<std::string> lines = splitLines(saved);
  if (lines.size() < 2 || lines[0] != "chessmagnet 1") return false;
  const std::string& currentFen = lines[1];

  // Preferred path: replay the recent moves from the anchor position, so
  // repetition detection and the last-move highlight survive a reboot.
  if (lines.size() >= 4) {
    Game g;
    if (g.setFen(lines[2])) {
      bool ok = true;
      size_t start = 0;
      const std::string& moves = lines[3];
      while (ok && start < moves.size()) {
        size_t sp = moves.find(' ', start);
        if (sp == std::string::npos) sp = moves.size();
        const std::string uci = moves.substr(start, sp - start);
        start = sp + 1;
        if (uci.empty()) continue;
        ok = false;
        for (const Move& m : g.legal_) {
          if (chess::uci::moveToUci(m) == uci) {
            g.history_.push_back(uci);
            g.lastFrom_ = m.from().index();
            g.lastTo_ = g.uiTarget(m);
            g.board_.makeMove(m);
            g.refresh();
            ok = true;
            break;
          }
        }
      }
      if (ok && g.board_.getFen() == currentFen) {
        *this = g;
        anchorFen_ = lines[2];
        return true;
      }
    }
  }
  // Fallback: the FEN alone (no history).
  return setFen(currentFen);
}

}  // namespace cm
