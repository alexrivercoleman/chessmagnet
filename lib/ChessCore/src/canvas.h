// A tiny software renderer that draws into an RGB565 pixel buffer.
//
// The same code draws into the ESP32's PSRAM frame buffer and into a plain
// array on the PC (which the preview tool saves as PNG), so what you see in
// the preview is pixel-for-pixel what the panel shows.
//
// Rotation: when setFlipped(true) is active, every drawing call is rotated
// 180 degrees (logical pixel x,y lands on physical W-1-x, H-1-y). The UI is
// always laid out "for the person holding it", and flipping decides which
// end of the device that person is at.
#pragma once
#include <stdint.h>

#include "font.h"

namespace cm {

constexpr int kScreenW = 480;
constexpr int kScreenH = 480;

constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return static_cast<uint16_t>(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

class Canvas {
 public:
  Canvas(uint16_t* pixels, int width, int height) : px_(pixels), w_(width), h_(height) {}

  void setFlipped(bool flipped) { flipped_ = flipped; }
  bool flipped() const { return flipped_; }
  int width() const { return w_; }
  int height() const { return h_; }
  uint16_t* pixels() { return px_; }

  void fillRect(int x, int y, int w, int h, uint16_t color);
  void blendRect(int x, int y, int w, int h, uint16_t color, uint8_t alpha);
  // Rounded rectangle with antialiased corners.
  void fillRoundRect(int x, int y, int w, int h, int radius, uint16_t color, uint8_t alpha = 255);
  // Antialiased disc / ring centred on (cx, cy).
  void fillCircle(float cx, float cy, float r, uint16_t color, uint8_t alpha = 255);
  void ring(float cx, float cy, float rOuter, float rInner, uint16_t color, uint8_t alpha = 255);
  // Sprite with straight (non-premultiplied) 8-bit alpha.
  void drawSprite(int x, int y, int w, int h, const uint16_t* rgb, const uint8_t* alpha);

  int textWidth(const Font& font, const char* text) const;
  void drawText(const Font& font, int x, int baseline, const char* text, uint16_t color);
  void drawTextCentered(const Font& font, int cx, int baseline, const char* text, uint16_t color);

  // Blend one logical pixel. alpha 0..255.
  void blendPixel(int x, int y, uint16_t color, uint8_t alpha);

 private:
  uint16_t* px_;
  int w_, h_;
  bool flipped_ = false;
};

// Mix two RGB565 colours; alpha=255 gives `src`.
uint16_t blend565(uint16_t dst, uint16_t src, uint8_t alpha);

}  // namespace cm
