// Behaviour tests for the game + UI logic, driven by fake touches exactly as
// the firmware would drive it.
#include <cstdio>
#include <string>

#include "desktop_support.h"

using namespace cm;

static int g_failures = 0;
static int g_checks = 0;
#define CHECK(cond)                                                     \
  do {                                                                  \
    ++g_checks;                                                         \
    if (!(cond)) {                                                      \
      ++g_failures;                                                     \
      std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);     \
    }                                                                   \
  } while (0)

static int sq(const char* name) { return (name[1] - '1') * 8 + (name[0] - 'a'); }

struct Rig {
  desk::MemoryStorage storage;
  desk::RecordingAudio audio;
  App app{storage, &audio};
  uint32_t t = 1000;

  void begin() { app.begin(t); }
  void tapAt(int x, int y) {
    app.touch(true, x, y, t);
    t += 80;
    app.touch(false, x, y, t);
    t += 200;
  }
  void tap(const char* square) {
    int x, y;
    app.squareCentre(sq(square), x, y);
    tapAt(x, y);
  }
  void move(const char* from, const char* to) {
    tap(from);
    tap(to);
  }
  void hold(uint32_t ms) {
    app.touch(true, 240, 240, t);
    for (uint32_t e = 0; e < ms; e += 20) {
      t += 20;
      app.touch(true, 240, 240, t);
    }
    app.touch(false, 240, 240, t);
    t += 100;
  }
  void loadFen(const std::string& fen) {
    storage.data[App::kSaveKey] = "chessmagnet 1\n" + fen + "\n" + fen + "\n\n";
    begin();
  }
};

static void testSelectAndMove() {
  std::puts("select, move, rotate");
  Rig r;
  r.begin();
  CHECK(r.app.viewWhite());
  r.tap("e2");
  CHECK(r.app.selected() == sq("e2"));
  r.tap("e4");
  CHECK(r.app.game().pieceAt(sq("e4")) == 0);  // white pawn
  CHECK(r.app.game().pieceAt(sq("e2")) == -1);
  CHECK(r.app.selected() == -1);
  CHECK(!r.app.viewWhite());  // view turned to Black
  CHECK(r.app.game().lastFrom() == sq("e2") && r.app.game().lastTo() == sq("e4"));
  CHECK(r.audio.played.size() == 1 && r.audio.played[0] == Sound::Move);
  CHECK(r.storage.writes == 1);
  CHECK(r.storage.data[App::kSaveKey].find("4P3") != std::string::npos);

  // The squares stay put on the glass; only the drawing turns around.
  int bx, by;
  r.app.squareCentre(sq("e7"), bx, by);
  Rig w;
  w.begin();
  int wx, wy;
  w.app.squareCentre(sq("e7"), wx, wy);
  CHECK(bx / 60 == wx / 60 && by / 60 == wy / 60);  // same square on the glass

  // Reselecting and deselecting.
  r.tap("g8");
  CHECK(r.app.selected() == sq("g8"));
  r.tap("b8");
  CHECK(r.app.selected() == sq("b8"));
  r.tap("b8");
  CHECK(r.app.selected() == -1);
  r.tap("e2");  // empty square: nothing happens
  CHECK(r.app.selected() == -1);
  r.tap("e4");  // opponent's piece: not selectable
  CHECK(r.app.selected() == -1);
  r.move("d7", "d5");
  r.move("e4", "d5");
  CHECK(r.audio.played.back() == Sound::Capture);
}

static void testIllegalMoveIgnored() {
  std::puts("illegal moves are ignored");
  Rig r;
  r.begin();
  r.move("e2", "e5");  // three squares: not legal
  CHECK(r.app.game().pieceAt(sq("e2")) == 0);
  CHECK(r.app.viewWhite());
  CHECK(r.storage.writes == 0);
}

static void testRestore() {
  std::puts("save and restore");
  Rig r;
  r.begin();
  r.move("e2", "e4");
  r.move("c7", "c5");
  r.move("g1", "f3");
  const std::string fen = r.app.game().fen();

  App rebooted(r.storage, nullptr);
  rebooted.begin(0);
  CHECK(rebooted.game().fen() == fen);
  CHECK(!rebooted.viewWhite());
  CHECK(rebooted.game().lastFrom() == sq("g1") && rebooted.game().lastTo() == sq("f3"));

  desk::MemoryStorage junk;
  junk.data[App::kSaveKey] = "not a saved game";
  App fresh(junk, nullptr);
  fresh.begin(0);
  CHECK(fresh.game().fen() == Game().fen());
}

static void testRepetitionSurvivesReboot() {
  std::puts("threefold repetition across a reboot");
  Rig r;
  r.begin();
  const char* moves[][2] = {{"g1", "f3"}, {"g8", "f6"}, {"f3", "g1"}, {"f6", "g8"},
                            {"g1", "f3"}, {"g8", "f6"}, {"f3", "g1"}};
  for (auto& m : moves) r.move(m[0], m[1]);
  CHECK(r.app.game().outcome() == Outcome::Ongoing);

  Rig again;  // "reboot": a new app reading the same flash
  again.storage = r.storage;
  again.begin();
  again.move("f6", "g8");  // start position for the third time
  CHECK(again.app.game().outcome() == Outcome::ThreefoldRepetition);
  CHECK(again.app.mode() == Mode::GameOver);
  CHECK(again.audio.played.back() == Sound::Draw);
}

static void testCastlingByKingTarget() {
  std::puts("castling by tapping the king's destination");
  Rig r;
  r.loadFen("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
  r.move("e1", "g1");
  CHECK(r.app.game().pieceAt(sq("g1")) == 5);  // white king
  CHECK(r.app.game().pieceAt(sq("f1")) == 3);  // white rook
  r.move("e8", "c8");
  CHECK(r.app.game().pieceAt(sq("c8")) == 11);
  CHECK(r.app.game().pieceAt(sq("d8")) == 9);
}

static void testPromotion() {
  std::puts("promotion picker");
  Rig r;
  // (the h2 pawn keeps K+N vs K from being an instant insufficient-material draw)
  r.loadFen("7k/P7/8/8/8/8/7P/K7 w - - 0 1");
  r.move("a7", "a8");
  CHECK(r.app.mode() == Mode::Promotion);
  CHECK(r.app.game().pieceAt(sq("a7")) == 0);  // nothing played yet
  r.tapAt(400, 400);  // outside the picker: cancel
  CHECK(r.app.mode() == Mode::Playing);
  CHECK(r.app.game().pieceAt(sq("a7")) == 0);
  r.move("a7", "a8");
  int x, y;
  r.app.promoChoiceCentre(3, x, y);  // knight
  r.tapAt(x, y);
  CHECK(r.app.game().pieceAt(sq("a8")) == 1);  // white knight
  CHECK(r.app.mode() == Mode::Playing);

  Rig b;  // Black promotes, viewed from Black's side
  b.loadFen("k7/8/8/8/8/8/7p/K7 b - - 0 1");
  CHECK(!b.app.viewWhite());
  b.move("h2", "h1");
  CHECK(b.app.mode() == Mode::Promotion);
  b.app.promoChoiceCentre(0, x, y);  // queen
  b.tapAt(x, y);
  CHECK(b.app.game().pieceAt(sq("h1")) == 10);  // black queen
}

static void testCheckmate() {
  std::puts("checkmate screen");
  Rig r;
  r.begin();
  r.move("f2", "f3");
  r.move("e7", "e5");
  r.move("g2", "g4");
  r.move("d8", "h4");
  CHECK(r.app.game().outcome() == Outcome::Checkmate);
  CHECK(r.app.mode() == Mode::GameOver);
  CHECK(!r.app.game().whiteWon());
  CHECK(!r.app.viewWhite());  // board didn't turn: still facing Black, the winner
  CHECK(r.audio.played.back() == Sound::Win);
  r.tap("e2");  // no moves after the game ends
  CHECK(r.app.selected() == -1);

  App rebooted(r.storage, nullptr);  // the result survives a reboot too
  rebooted.begin(0);
  CHECK(rebooted.mode() == Mode::GameOver);
  CHECK(!rebooted.viewWhite());
}

static void testDraws() {
  std::puts("stalemate and insufficient material");
  Rig s;
  s.loadFen("7k/4Q3/6K1/8/8/8/8/8 w - - 0 1");
  s.move("e7", "f7");
  CHECK(s.app.game().outcome() == Outcome::Stalemate);
  CHECK(s.app.mode() == Mode::GameOver);

  Rig m;
  m.loadFen("7k/8/8/8/8/8/1r6/K7 w - - 0 1");
  m.move("a1", "b2");
  CHECK(m.app.game().outcome() == Outcome::InsufficientMaterial);
}

static void testLongPressNewGame() {
  std::puts("long-press new game with confirmation");
  Rig r;
  r.begin();
  r.move("e2", "e4");
  r.hold(1200);  // not long enough
  CHECK(r.app.mode() == Mode::Playing);
  r.hold(2100);
  CHECK(r.app.mode() == Mode::ConfirmNewGame);
  int x, y;
  r.app.buttonCentre(false, x, y);  // Cancel
  r.tapAt(x, y);
  CHECK(r.app.mode() == Mode::Playing);
  CHECK(r.app.game().pieceAt(sq("e4")) == 0);
  r.hold(2100);
  r.app.buttonCentre(true, x, y);  // New game
  r.tapAt(x, y);
  CHECK(r.app.mode() == Mode::Playing);
  CHECK(r.app.game().fen() == Game().fen());
  CHECK(r.app.viewWhite());
}

static void testPowerStates() {
  std::puts("screen off, wake on touch, deep sleep");
  Rig r;
  r.begin();
  r.app.update(r.t + 2 * 60 * 1000);
  CHECK(r.app.power() == Power::On);
  r.t += 3 * 60 * 1000 + 1;
  r.app.update(r.t);
  CHECK(r.app.power() == Power::ScreenOff);
  r.tap("e2");  // wakes the screen but must not select anything
  CHECK(r.app.power() == Power::On);
  CHECK(r.app.selected() == -1);
  r.tap("e2");
  CHECK(r.app.selected() == sq("e2"));
  r.t += 60 * 60 * 1000;
  r.app.update(r.t);
  CHECK(r.app.power() == Power::DeepSleep);
}

static void testRenderSmoke() {
  std::puts("render smoke test");
  Rig r;
  r.begin();
  static uint16_t fb[kScreenW * kScreenH];
  Canvas c(fb, kScreenW, kScreenH);
  CHECK(r.app.needsRedraw());
  r.app.render(c);
  CHECK(!r.app.needsRedraw());
  // a1 is dark and drawn bottom-left for White; its corner pixel shows the square colour.
  CHECK(fb[(kScreenH - 1) * kScreenW + 59] == rgb565(118, 150, 86));
  r.move("e2", "e4");
  r.app.render(c);
  // With Black to move the image is rotated: a1 (dark) is still bottom-left on the glass.
  CHECK(fb[(kScreenH - 1) * kScreenW + 59] == rgb565(118, 150, 86));
}

int main() {
  testSelectAndMove();
  testIllegalMoveIgnored();
  testRestore();
  testRepetitionSurvivesReboot();
  testCastlingByKingTarget();
  testPromotion();
  testCheckmate();
  testDraws();
  testLongPressNewGame();
  testPowerStates();
  testRenderSmoke();
  if (g_failures) {
    std::printf("\n%d of %d checks FAILED\n", g_failures, g_checks);
  } else {
    std::printf("\nAll %d checks passed.\n", g_checks);
  }
  return g_failures ? 1 : 0;
}
