// App drawing code. Every frame is drawn from scratch: a 480x480 software
// render takes a few milliseconds and keeps the code simple.
#include <stdio.h>

#include "app.h"
#include "gen/fonts.h"
#include "gen/pieces.h"
#include "layout.h"

namespace cm {

static_assert(gen::kPieceSize == kSquare, "piece sprites must match the square size");

namespace {
void drawPiece(Canvas& c, int piece, int x, int y) {
  c.drawSprite(x, y, gen::kPieceSize, gen::kPieceSize, gen::kPieceRGB565[piece], gen::kPieceAlpha[piece]);
}

const char* titleFor(Outcome o) {
  switch (o) {
    case Outcome::Checkmate: return "Checkmate";
    case Outcome::Stalemate: return "Stalemate";
    default: return "Draw";
  }
}

const char* subtitleFor(const Game& g) {
  switch (g.outcome()) {
    case Outcome::Checkmate: return g.whiteWon() ? "White wins" : "Black wins";
    case Outcome::InsufficientMaterial: return "Not enough pieces to checkmate";
    case Outcome::FiftyMoveRule: return "50 moves without a capture or pawn move";
    case Outcome::ThreefoldRepetition: return "Same position three times";
    default: return "";
  }
}
}  // namespace

void App::render(Canvas& c) {
  dirty_ = false;
  c.setFlipped(!viewWhite_);
  drawBoard(c);
  switch (mode_) {
    case Mode::Promotion: drawPromotion(c); break;
    case Mode::ConfirmNewGame: drawConfirm(c); break;
    case Mode::GameOver:
      if (!resultHidden_) drawGameOver(c);
      break;
    case Mode::Playing: break;
  }
  if (holdStepDrawn_ >= 0 && mode_ != Mode::ConfirmNewGame) {
    c.setFlipped(!viewWhite_);
    drawHoldHint(c);
  }
}

void App::drawBoard(Canvas& c) const {
  const int checked = game_.checkedKing();
  for (int sq = 0; sq < 64; ++sq) {
    int x, y;
    squareOrigin(sq, x, y);
    const bool light = ((sq % 8) + (sq / 8)) % 2 == 1;  // a1 is dark
    c.fillRect(x, y, kSquare, kSquare, light ? kLightSquare : kDarkSquare);
    if (sq == selected_) {
      c.blendRect(x, y, kSquare, kSquare, kHighlight, kSelectedAlpha);
    } else if (sq == game_.lastFrom() || sq == game_.lastTo()) {
      c.blendRect(x, y, kSquare, kSquare, kHighlight, kLastMoveAlpha);
    }
    if (sq == checked) {  // red glow under a king in check
      c.fillCircle(x + 30.f, y + 30.f, 29.f, kCheck, 90);
      c.fillCircle(x + 30.f, y + 30.f, 21.f, kCheck, 150);
    }
    const int p = game_.pieceAt(sq);
    if (p >= 0) drawPiece(c, p, x, y);
    if ((targets_ >> sq) & 1) {
      if (p >= 0) {
        c.ring(x + 30.f, y + 30.f, 29.5f, 24.f, kHint, kHintAlpha);  // capture
      } else {
        c.fillCircle(x + 30.f, y + 30.f, 9.5f, kHint, kHintAlpha);  // quiet move
      }
    }
  }
  // Coordinates: files along the bottom row, ranks up the left column.
  for (int i = 0; i < 8; ++i) {
    const int file = viewWhite_ ? i : 7 - i;
    const int rank = viewWhite_ ? 7 - i : i;
    char label[2] = {static_cast<char>('a' + file), 0};
    // Bottom row square of this column: colour decides the label colour.
    const bool bottomLight = ((file + (viewWhite_ ? 0 : 7)) % 2) == 1;
    c.drawText(gen::kTiny, i * kSquare + kSquare - 10, kScreenH - 4, label, bottomLight ? kDarkSquare : kLightSquare);
    label[0] = static_cast<char>('1' + rank);
    const bool leftLight = (((viewWhite_ ? 0 : 7) + rank) % 2) == 1;
    c.drawText(gen::kTiny, 3, i * kSquare + 14, label, leftLight ? kDarkSquare : kLightSquare);
  }
}

void App::drawPromotion(Canvas& c) const {
  c.blendRect(0, 0, kScreenW, kScreenH, 0, 120);  // dim the board
  int x, y;
  squareOrigin(promoTo_, x, y);
  const bool white = game_.whiteToMove();
  const int types[4] = {4, 3, 2, 1};  // queen, rook, bishop, knight (chess::PieceType order)
  for (int i = 0; i < 4; ++i) {
    const int py = i * kSquare;
    c.fillRoundRect(x + 2, py + 2, kSquare - 4, kSquare - 4, 10, kPromoBg);
    drawPiece(c, types[i] + (white ? 0 : 6), x, py);
  }
}

void App::drawConfirm(Canvas& c) const {
  c.blendRect(0, 0, kScreenW, kScreenH, 0, 140);
  const Rect& p = kConfirmPanel;
  c.fillRoundRect(p.x, p.y, p.w, p.h, 18, kPanel, 245);
  c.drawTextCentered(gen::kLarge, kScreenW / 2, p.y + 66, "New game?", kText);
  c.fillRoundRect(kConfirmNo.x, kConfirmNo.y, kConfirmNo.w, kConfirmNo.h, 12, kButton);
  c.drawTextCentered(gen::kSmall, kConfirmNo.x + kConfirmNo.w / 2, kConfirmNo.y + 40, "Cancel", kText);
  c.fillRoundRect(kConfirmYes.x, kConfirmYes.y, kConfirmYes.w, kConfirmYes.h, 12, kButtonGo);
  c.drawTextCentered(gen::kSmall, kConfirmYes.x + kConfirmYes.w / 2, kConfirmYes.y + 40, "New game", kText);
}

void App::drawGameOver(Canvas& c) const {
  c.blendRect(0, 0, kScreenW, kScreenH, 0, 90);
  // Faces the same way as the board (after checkmate, that's the winner).
  const char* subtitle = subtitleFor(game_);
  const int top = (kScreenH - 150) / 2;
  c.fillRoundRect(24, top, kScreenW - 48, 150, 18, kPanel, 240);
  if (*subtitle) {
    c.drawTextCentered(gen::kLarge, kScreenW / 2, top + 52, titleFor(game_.outcome()), kText);
    c.drawTextCentered(gen::kSmall, kScreenW / 2, top + 92, subtitle, kTextDim);
  } else {
    c.drawTextCentered(gen::kLarge, kScreenW / 2, top + 72, titleFor(game_.outcome()), kText);
  }
  c.drawTextCentered(gen::kTiny, kScreenW / 2, top + 130, "Tap to see the board  -  hold 2 s for a new game", kTextDim);
}

void App::drawHoldHint(Canvas& c) const {
  const int step = holdStepDrawn_ < 24 ? holdStepDrawn_ : 24;
  const Rect panel{60, 196, 360, 88};
  c.fillRoundRect(panel.x, panel.y, panel.w, panel.h, 16, kPanel, 235);
  c.drawTextCentered(gen::kSmall, kScreenW / 2, panel.y + 36, "Keep holding for a new game", kText);
  const int barW = panel.w - 48;
  c.fillRoundRect(panel.x + 24, panel.y + 54, barW, 14, 7, kButton);
  const int fill = barW * step / 24;
  if (fill >= 14) c.fillRoundRect(panel.x + 24, panel.y + 54, fill, 14, 7, kButtonGo);
}

}  // namespace cm
