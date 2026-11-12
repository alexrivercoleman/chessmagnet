#include "sound.h"

#include <math.h>

namespace cm {
namespace synth {

namespace {
constexpr float kPi = 3.14159265f;

// Deterministic white noise (same sound every time, no <random> needed).
struct Noise {
  uint32_t s = 0x1234567u;
  float next() {
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return static_cast<float>(s & 0xFFFF) / 32768.f - 1.f;
  }
};

void ensureLength(std::vector<float>& buf, size_t n) {
  if (buf.size() < n) buf.resize(n, 0.f);
}

// A struck wooden piece: a short noise click plus two decaying resonances.
void knock(std::vector<float>& buf, float startSec, float pitchHz, float decaySec, float gain) {
  const size_t start = static_cast<size_t>(startSec * kSampleRate);
  const size_t len = static_cast<size_t>((decaySec * 6.f + 0.004f) * kSampleRate);
  ensureLength(buf, start + len);
  Noise noise;
  float lp = 0.f;
  for (size_t i = 0; i < len; ++i) {
    const float t = static_cast<float>(i) / kSampleRate;
    const float env = expf(-t / decaySec);
    const float tone = 0.7f * sinf(2 * kPi * pitchHz * t) + 0.3f * sinf(2 * kPi * pitchHz * 2.76f * t) * expf(-t / (decaySec * 0.4f));
    lp += 0.35f * (noise.next() - lp);  // soften the click
    const float click = lp * expf(-t / 0.0025f);
    buf[start + i] += gain * (env * tone + 0.9f * click);
  }
}

// A plain musical note with a soft attack and release.
void note(std::vector<float>& buf, float startSec, float durSec, float hz, float gain) {
  const size_t start = static_cast<size_t>(startSec * kSampleRate);
  const size_t len = static_cast<size_t>(durSec * kSampleRate);
  ensureLength(buf, start + len);
  for (size_t i = 0; i < len; ++i) {
    const float t = static_cast<float>(i) / kSampleRate;
    const float attack = t < 0.01f ? t / 0.01f : 1.f;
    const float release = expf(-t / (durSec * 0.45f));
    // Fundamental plus a little 2nd/3rd harmonic: a bright, bell-ish tone.
    const float s = sinf(2 * kPi * hz * t) + 0.3f * sinf(4 * kPi * hz * t) + 0.12f * sinf(6 * kPi * hz * t);
    buf[start + i] += gain * attack * release * s;
  }
}

float hzOf(int midi) { return 440.f * powf(2.f, (midi - 69) / 12.f); }
}  // namespace

void render(Sound s, std::vector<int16_t>& out) {
  std::vector<float> buf;
  switch (s) {
    case Sound::Move:
      knock(buf, 0.f, 820.f, 0.012f, 0.55f);
      break;
    case Sound::Capture:
      knock(buf, 0.f, 560.f, 0.018f, 0.7f);
      knock(buf, 0.055f, 700.f, 0.014f, 0.6f);
      break;
    case Sound::Win: {
      // An original short fanfare: rising arpeggio, then a held chord.
      const int run[] = {67, 72, 76, 79};  // G4 C5 E5 G5
      for (int i = 0; i < 4; ++i) note(buf, i * 0.11f, 0.22f, hzOf(run[i]), 0.28f);
      const int chord[] = {72, 76, 79, 84};  // C major, C5..C6
      for (int n : chord) note(buf, 0.48f, 1.1f, hzOf(n), 0.16f);
      break;
    }
    case Sound::Draw:
      note(buf, 0.f, 0.35f, hzOf(76), 0.3f);     // E5
      note(buf, 0.28f, 0.6f, hzOf(72), 0.3f);    // C5
      break;
  }
  out.resize(buf.size());
  for (size_t i = 0; i < buf.size(); ++i) {
    float v = buf[i];
    if (v > 1.f) v = 1.f;
    if (v < -1.f) v = -1.f;
    out[i] = static_cast<int16_t>(v * 26000.f);
  }
}

}  // namespace synth
}  // namespace cm
