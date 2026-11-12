#include "app.h"

#include "layout.h"

namespace cm {

App::App(Storage& storage, AudioOut* audio, AppConfig cfg) : storage_(storage), audio_(audio), cfg_(cfg) {}

void App::begin(uint32_t nowMs) {
  now_ = lastActivity_ = nowMs;
  std::string saved;
  if (!(storage_.load(kSaveKey, saved) && game_.restore(saved))) game_.reset();
  viewWhite_ = game_.outcome() == Outcome::Checkmate ? game_.whiteWon() : game_.whiteToMove();
  mode_ = game_.outcome() == Outcome::Ongoing ? Mode::Playing : Mode::GameOver;
  power_ = Power::On;
  dirty_ = true;
}

// ---------------------------------------------------------------- touch input

void App::touch(bool pressed, int x, int y, uint32_t nowMs) {
  if (pressed && !down_) {  // finger just landed
    down_ = true;
    downX_ = x;
    downY_ = y;
    downAt_ = nowMs;
    longFired_ = false;
    holdStepDrawn_ = -1;
    // A touch on a dark screen only wakes it; it must not also move a piece.
    swallow_ = power_ != Power::On;
    if (swallow_) {
      power_ = Power::On;
      dirty_ = true;
    }
    lastActivity_ = nowMs;
  } else if (pressed) {  // still holding
    lastActivity_ = nowMs;
  } else if (down_) {  // finger lifted
    down_ = false;
    lastActivity_ = nowMs;
    if (holdStepDrawn_ >= 0) dirty_ = true;  // remove the "keep holding" bar
    holdStepDrawn_ = -1;
    if (!swallow_ && !longFired_ && nowMs - downAt_ <= cfg_.tapMaxMs) {
      int lx, ly;
      toLogical(downX_, downY_, lx, ly);
      onTap(lx, ly);
    }
    swallow_ = false;
  }
  update(nowMs);
}

int App::holdStep(uint32_t nowMs) const {
  const uint32_t held = nowMs - downAt_;
  if (held < cfg_.holdHintMs) return -1;
  const uint32_t span = cfg_.longPressMs - cfg_.holdHintMs;
  return static_cast<int>((held - cfg_.holdHintMs) * 24 / (span ? span : 1));  // 0..24
}

void App::update(uint32_t nowMs) {
  now_ = nowMs;
  if (down_ && !swallow_ && !longFired_) {
    if (nowMs - downAt_ >= cfg_.longPressMs) {
      longFired_ = true;
      holdStepDrawn_ = -1;
      onLongPress();
    } else if (mode_ != Mode::ConfirmNewGame) {
      const int step = holdStep(nowMs);
      if (step != holdStepDrawn_) {  // redraw only when the bar visibly grows
        holdStepDrawn_ = step;
        dirty_ = true;
      }
    }
  }
  if (!down_) {
    // Unsigned subtraction keeps working when millis() wraps after 49 days.
    const uint32_t idle = nowMs - lastActivity_;
    if (idle >= cfg_.deepSleepMs) {
      power_ = Power::DeepSleep;
    } else if (idle >= cfg_.screenOffMs && power_ == Power::On) {
      power_ = Power::ScreenOff;
    }
  }
}

// ---------------------------------------------------------------- geometry

void App::toLogical(int px, int py, int& lx, int& ly) const {
  // The board is drawn rotated 180 degrees when Black is to move, so touches
  // are rotated the same way to land on the square the player sees.
  if (viewWhite_) {
    lx = px;
    ly = py;
  } else {
    lx = kScreenW - 1 - px;
    ly = kScreenH - 1 - py;
  }
}

void App::squareOrigin(int sq, int& lx, int& ly) const {
  // Logical view = the player to move sits at the bottom.
  const int file = sq % 8, rank = sq / 8;
  const int col = viewWhite_ ? file : 7 - file;
  const int row = viewWhite_ ? 7 - rank : rank;
  lx = col * kSquare;
  ly = row * kSquare;
}

int App::squareAtLogical(int lx, int ly) const {
  if (lx < 0 || ly < 0 || lx >= kScreenW || ly >= kScreenH) return -1;
  const int col = lx / kSquare, row = ly / kSquare;
  const int file = viewWhite_ ? col : 7 - col;
  const int rank = viewWhite_ ? 7 - row : row;
  return rank * 8 + file;
}

void App::squareCentre(int sq, int& x, int& y) const {
  int lx, ly;
  squareOrigin(sq, lx, ly);
  toLogical(lx + kSquare / 2, ly + kSquare / 2, x, y);  // the mapping is its own inverse
}

void App::buttonCentre(bool confirm, int& x, int& y) const {
  const Rect& r = confirm ? kConfirmYes : kConfirmNo;
  toLogical(r.x + r.w / 2, r.y + r.h / 2, x, y);
}

void App::promoChoiceCentre(int index, int& x, int& y) const {
  int lx, ly;
  squareOrigin(promoTo_ >= 0 ? promoTo_ : 0, lx, ly);
  toLogical(lx + kSquare / 2, index * kSquare + kSquare / 2, x, y);
}

// ---------------------------------------------------------------- actions

void App::onTap(int lx, int ly) {
  dirty_ = true;
  switch (mode_) {
    case Mode::ConfirmNewGame:
      if (kConfirmYes.contains(lx, ly)) {
        startNewGame();
      } else if (kConfirmNo.contains(lx, ly)) {
        mode_ = modeBeforeConfirm_;
      }
      return;

    case Mode::Promotion: {
      // The picker is a column of 4 squares hanging down from the promotion
      // square (which is always on the top row for the player to move).
      int sx, sy;
      squareOrigin(promoTo_, sx, sy);
      if (lx >= sx && lx < sx + kSquare && ly >= 0 && ly < 4 * kSquare) {
        finishMove(promoFrom_, promoTo_, "qrbn"[ly / kSquare]);
      } else {  // tapped outside the picker: cancel the move
        mode_ = Mode::Playing;
        selected_ = -1;
        targets_ = 0;
      }
      return;
    }

    case Mode::GameOver:
      resultHidden_ = !resultHidden_;  // let players look at the final position
      return;

    case Mode::Playing:
      tapBoard(squareAtLogical(lx, ly));
      return;
  }
}

void App::tapBoard(int sq) {
  if (sq < 0) return;
  if (selected_ >= 0 && ((targets_ >> sq) & 1)) {
    if (game_.needsPromotion(selected_, sq)) {
      promoFrom_ = selected_;
      promoTo_ = sq;
      mode_ = Mode::Promotion;
    } else {
      finishMove(selected_, sq, 'q');
    }
    return;
  }
  const int p = game_.pieceAt(sq);
  const bool own = p >= 0 && ((p < 6) == game_.whiteToMove());
  if (own && sq != selected_) {
    selected_ = sq;
    targets_ = game_.targetsFrom(sq);
  } else {  // tapping the selected piece again, or an empty/enemy square
    selected_ = -1;
    targets_ = 0;
  }
}

void App::finishMove(int from, int to, char promo) {
  MoveResult r;
  selected_ = -1;
  targets_ = 0;
  mode_ = Mode::Playing;
  if (!game_.play(from, to, promo, &r)) return;
  if (audio_) audio_->play(r.capture ? Sound::Capture : Sound::Move);
  save();
  // Turn the view to face the next player, except after checkmate: then the
  // board stays facing the winner, who just moved.
  viewWhite_ = game_.outcome() == Outcome::Checkmate ? game_.whiteWon() : game_.whiteToMove();
  if (game_.outcome() != Outcome::Ongoing) {
    mode_ = Mode::GameOver;
    resultHidden_ = false;
    if (audio_) audio_->play(game_.outcome() == Outcome::Checkmate ? Sound::Win : Sound::Draw);
  }
}

void App::onLongPress() {
  if (mode_ == Mode::ConfirmNewGame) return;
  modeBeforeConfirm_ = mode_ == Mode::GameOver ? Mode::GameOver : Mode::Playing;
  selected_ = -1;
  targets_ = 0;
  mode_ = Mode::ConfirmNewGame;
  dirty_ = true;
}

void App::startNewGame() {
  game_.reset();
  save();
  selected_ = -1;
  targets_ = 0;
  viewWhite_ = true;
  resultHidden_ = false;
  mode_ = Mode::Playing;
}

void App::save() { storage_.save(kSaveKey, game_.serialize()); }

}  // namespace cm
