// Sound effects, synthesised in code (no audio files needed).
#pragma once
#include <stdint.h>

#include <vector>

namespace cm {

enum class Sound : uint8_t {
  Move,     // a soft wooden "tock"
  Capture,  // a heavier double knock
  Win,      // played on checkmate (the firmware plays your win.wav instead if flashed)
  Draw,     // two gentle falling notes
};

// Implemented by the firmware (I2S speaker) and by the desktop tests (a log).
class AudioOut {
 public:
  virtual ~AudioOut() = default;
  virtual void play(Sound s) = 0;  // must not block for long
};

namespace synth {
constexpr int kSampleRate = 16000;  // plenty for a small speaker; halves memory vs 32 kHz
// Fills `out` with 16-bit mono samples at kSampleRate.
void render(Sound s, std::vector<int16_t>& out);
}  // namespace synth

}  // namespace cm
