// Renders a set of screens to PNG files (exactly the pixels the panel would
// show) and the sound effects to WAV files, so the look and sound can be
// checked before the hardware arrives.
//
// Usage: preview.exe <output folder>
#include <cstdio>
#include <string>
#include <vector>

#include "desktop_support.h"

using namespace cm;

static uint16_t g_fb[kScreenW * kScreenH];

static int sq(const char* n) { return (n[1] - '1') * 8 + (n[0] - 'a'); }

struct Shot {
  desk::MemoryStorage storage;
  App app{storage, nullptr};
  uint32_t t = 1000;

  explicit Shot(const char* fen = nullptr) {
    if (fen) storage.data[App::kSaveKey] = std::string("chessmagnet 1\n") + fen + "\n" + fen + "\n\n";
    app.begin(t);
  }
  void tapAt(int x, int y) {
    app.touch(true, x, y, t);
    t += 80;
    app.touch(false, x, y, t);
    t += 200;
  }
  void tap(const char* s) {
    int x, y;
    app.squareCentre(sq(s), x, y);
    tapAt(x, y);
  }
  void move(const char* a, const char* b) {
    tap(a);
    tap(b);
  }
  void save(const std::string& dir, const char* name) {
    Canvas c(g_fb, kScreenW, kScreenH);
    app.render(c);
    const std::string path = dir + "/" + name + ".png";
    std::printf("%s %s\n", desk::writePng(path.c_str(), g_fb, kScreenW, kScreenH) ? "wrote" : "FAILED", path.c_str());
  }
};

int main(int argc, char** argv) {
  const std::string dir = argc > 1 ? argv[1] : ".";

  { Shot s; s.save(dir, "01_start"); }
  {
    Shot s;
    s.move("e2", "e4");
    s.move("e7", "e5");
    s.tap("g1");
    s.save(dir, "02_knight_selected");
  }
  {
    Shot s;
    s.move("e2", "e4");
    s.save(dir, "03_black_to_move_rotated");
  }
  {
    Shot s;
    s.move("e2", "e4");
    s.move("d7", "d5");
    s.tap("e4");
    s.save(dir, "04_capture_available");
  }
  {
    Shot s("r1bqkbnr/pppp1ppp/2n5/4p2Q/2B1P3/8/PPPP1PPP/RNB1K1NR w KQkq - 2 4");
    s.move("h5", "f7");
    s.save(dir, "05_checkmate");
  }
  {
    Shot s("rnbqkbnr/ppppp1pp/5p2/7Q/4P3/8/PPPP1PPP/RNB1KBNR b KQkq - 1 2");  // 1.e4 f6 2.Qh5+
    s.save(dir, "06_in_check");
  }
  {
    Shot s("7k/P7/8/8/8/8/8/K7 w - - 0 1");
    s.move("a7", "a8");
    s.save(dir, "07_promotion");
  }
  {
    Shot s;
    s.move("e2", "e4");
    s.app.touch(true, 240, 240, s.t);
    s.app.touch(true, 240, 240, s.t + 1250);
    s.save(dir, "08_hold_for_new_game");
    s.app.touch(true, 240, 240, s.t + 2100);
    s.app.touch(false, 240, 240, s.t + 2150);
    s.save(dir, "09_confirm_new_game");
  }
  {
    Shot s("7k/4Q3/6K1/8/8/8/8/8 w - - 0 1");
    s.move("e7", "f7");
    s.save(dir, "10_stalemate");
  }

  const struct {
    Sound s;
    const char* name;
  } sounds[] = {{Sound::Move, "move"}, {Sound::Capture, "capture"}, {Sound::Win, "win_fallback"}, {Sound::Draw, "draw"}};
  for (const auto& snd : sounds) {
    std::vector<int16_t> pcm;
    synth::render(snd.s, pcm);
    const std::string path = dir + "/sound_" + snd.name + ".wav";
    std::printf("%s %s\n", desk::writeWav(path.c_str(), pcm, synth::kSampleRate) ? "wrote" : "FAILED", path.c_str());
  }
  return 0;
}
