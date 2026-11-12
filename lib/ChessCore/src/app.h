// App: everything the player experiences, independent of hardware.
//
// The firmware (or a desktop test) feeds it touch samples and the current
// time, asks whether the screen needs redrawing, and reads back which power
// state it wants. All hardware lives behind the small interfaces below, so
// this whole file can be tested on a PC.
#pragma once
#include <stdint.h>

#include <string>

#include "canvas.h"
#include "game.h"
#include "sound.h"

namespace cm {

// Key-value storage (NVS flash on the ESP32, a map in tests).
class Storage {
 public:
  virtual ~Storage() = default;
  virtual bool load(const char* key, std::string& out) = 0;
  virtual bool save(const char* key, const std::string& value) = 0;
};

struct AppConfig {
  uint32_t tapMaxMs = 800;          // a touch shorter than this is a tap
  uint32_t holdHintMs = 500;        // after this, show the "keep holding" bar
  uint32_t longPressMs = 2000;      // hold this long for the new-game prompt
  uint32_t screenOffMs = 3 * 60 * 1000UL;   // backlight off after 3 min idle
  uint32_t deepSleepMs = 60 * 60 * 1000UL;  // deep sleep after 1 h idle
};

enum class Power : uint8_t { On, ScreenOff, DeepSleep };
enum class Mode : uint8_t { Playing, Promotion, ConfirmNewGame, GameOver };

class App {
 public:
  static constexpr const char* kSaveKey = "game";

  App(Storage& storage, AudioOut* audio, AppConfig cfg = AppConfig());

  // Restores the saved game (or starts a new one).
  void begin(uint32_t nowMs);

  // Call on every touch poll with the panel's coordinates (0..479).
  void touch(bool pressed, int x, int y, uint32_t nowMs);
  // Call regularly (e.g. every loop) to advance timers.
  void update(uint32_t nowMs);

  bool needsRedraw() const { return dirty_; }
  void render(Canvas& canvas);  // draws a full frame and clears needsRedraw()
  Power power() const { return power_; }

  // Read-only access for tests and the preview tool.
  const Game& game() const { return game_; }
  Game& gameForPreview() { return game_; }
  Mode mode() const { return mode_; }
  int selected() const { return selected_; }
  bool viewWhite() const { return viewWhite_; }
  // Physical touch point of the centre of a board square (for tests).
  void squareCentre(int sq, int& x, int& y) const;
  void buttonCentre(bool confirm, int& x, int& y) const;  // confirm-dialog buttons
  void promoChoiceCentre(int index, int& x, int& y) const;  // 0=Q 1=R 2=B 3=N

 private:
  void onTap(int lx, int ly);
  void onLongPress();
  void tapBoard(int sq);
  void finishMove(int from, int to, char promo);
  void startNewGame();
  void save();
  void toLogical(int px, int py, int& lx, int& ly) const;
  int squareAtLogical(int lx, int ly) const;
  void squareOrigin(int sq, int& lx, int& ly) const;  // top-left, logical coords
  int holdStep(uint32_t nowMs) const;

  // Drawing (render.cpp)
  void drawBoard(Canvas& c) const;
  void drawPromotion(Canvas& c) const;
  void drawConfirm(Canvas& c) const;
  void drawGameOver(Canvas& c) const;
  void drawHoldHint(Canvas& c) const;

  Storage& storage_;
  AudioOut* audio_;
  AppConfig cfg_;
  Game game_;

  Mode mode_ = Mode::Playing;
  Mode modeBeforeConfirm_ = Mode::Playing;
  bool viewWhite_ = true;     // whose perspective the board is drawn from
  int selected_ = -1;         // selected square or -1
  uint64_t targets_ = 0;      // legal destinations of the selected piece
  int promoFrom_ = -1, promoTo_ = -1;
  bool resultHidden_ = false; // game-over banner hidden (tap to toggle)

  // Touch tracking
  bool down_ = false;
  bool swallow_ = false;      // this touch only woke the screen
  bool longFired_ = false;
  int downX_ = 0, downY_ = 0;
  uint32_t downAt_ = 0;
  int holdStepDrawn_ = -1;
  uint32_t now_ = 0;

  uint32_t lastActivity_ = 0;
  Power power_ = Power::On;
  bool dirty_ = true;
};

}  // namespace cm
