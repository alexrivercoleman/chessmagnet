// Desktop-only helpers: in-memory stand-ins for the hardware, plus tiny PNG
// and WAV writers (no external libraries needed).
#pragma once
#include <stdint.h>
#include <stdio.h>

#include <map>
#include <string>
#include <vector>

#include "ChessCore.h"

namespace desk {

class MemoryStorage : public cm::Storage {
 public:
  bool load(const char* key, std::string& out) override {
    auto it = data.find(key);
    if (it == data.end()) return false;
    out = it->second;
    return true;
  }
  bool save(const char* key, const std::string& value) override {
    data[key] = value;
    ++writes;
    return true;
  }
  std::map<std::string, std::string> data;
  int writes = 0;
};

class RecordingAudio : public cm::AudioOut {
 public:
  void play(cm::Sound s) override { played.push_back(s); }
  std::vector<cm::Sound> played;
};

inline uint32_t crc32(const uint8_t* d, size_t n, uint32_t c = 0xFFFFFFFFu) {
  for (size_t i = 0; i < n; ++i) {
    c ^= d[i];
    for (int k = 0; k < 8; ++k) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
  }
  return c;
}

// Writes an RGB565 frame as a PNG (uncompressed "stored" deflate blocks).
inline bool writePng(const char* path, const uint16_t* px, int w, int h) {
  std::vector<uint8_t> raw;
  raw.reserve(static_cast<size_t>(h) * (1 + w * 3));
  for (int y = 0; y < h; ++y) {
    raw.push_back(0);  // filter: none
    for (int x = 0; x < w; ++x) {
      const uint16_t c = px[y * w + x];
      raw.push_back(static_cast<uint8_t>(((c >> 11) & 31) * 255 / 31));
      raw.push_back(static_cast<uint8_t>(((c >> 5) & 63) * 255 / 63));
      raw.push_back(static_cast<uint8_t>((c & 31) * 255 / 31));
    }
  }
  std::vector<uint8_t> z = {0x78, 0x01};
  for (size_t pos = 0; pos < raw.size() || pos == 0;) {
    const size_t n = raw.size() - pos > 65535 ? 65535 : raw.size() - pos;
    z.push_back(pos + n == raw.size() ? 1 : 0);
    z.push_back(n & 0xFF);
    z.push_back((n >> 8) & 0xFF);
    z.push_back(~n & 0xFF);
    z.push_back((~n >> 8) & 0xFF);
    z.insert(z.end(), raw.begin() + pos, raw.begin() + pos + n);
    pos += n;
    if (n == 0) break;
  }
  uint32_t a = 1, b = 0;
  for (uint8_t v : raw) {
    a = (a + v) % 65521;
    b = (b + a) % 65521;
  }
  const uint32_t adler = (b << 16) | a;
  for (int i = 3; i >= 0; --i) z.push_back((adler >> (8 * i)) & 0xFF);

  FILE* f = fopen(path, "wb");
  if (!f) return false;
  auto be32 = [&](uint32_t v) {
    uint8_t b4[4] = {uint8_t(v >> 24), uint8_t(v >> 16), uint8_t(v >> 8), uint8_t(v)};
    fwrite(b4, 1, 4, f);
  };
  auto chunk = [&](const char* type, const std::vector<uint8_t>& data) {
    be32(static_cast<uint32_t>(data.size()));
    std::vector<uint8_t> td(type, type + 4);
    td.insert(td.end(), data.begin(), data.end());
    fwrite(td.data(), 1, td.size(), f);
    be32(crc32(td.data(), td.size()) ^ 0xFFFFFFFFu);
  };
  const uint8_t sig[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
  fwrite(sig, 1, 8, f);
  std::vector<uint8_t> ihdr = {uint8_t(w >> 24), uint8_t(w >> 16), uint8_t(w >> 8), uint8_t(w),
                               uint8_t(h >> 24), uint8_t(h >> 16), uint8_t(h >> 8), uint8_t(h),
                               8, 2, 0, 0, 0};  // 8-bit RGB
  chunk("IHDR", ihdr);
  chunk("IDAT", z);
  chunk("IEND", {});
  fclose(f);
  return true;
}

inline bool writeWav(const char* path, const std::vector<int16_t>& s, int rate) {
  FILE* f = fopen(path, "wb");
  if (!f) return false;
  auto le32 = [&](uint32_t v) { fwrite(&v, 4, 1, f); };
  auto le16 = [&](uint16_t v) { fwrite(&v, 2, 1, f); };
  const uint32_t bytes = static_cast<uint32_t>(s.size() * 2);
  fwrite("RIFF", 1, 4, f);
  le32(36 + bytes);
  fwrite("WAVEfmt ", 1, 8, f);
  le32(16);
  le16(1);  // PCM
  le16(1);  // mono
  le32(rate);
  le32(rate * 2);
  le16(2);
  le16(16);
  fwrite("data", 1, 4, f);
  le32(bytes);
  fwrite(s.data(), 2, s.size(), f);
  fclose(f);
  return true;
}

}  // namespace desk
