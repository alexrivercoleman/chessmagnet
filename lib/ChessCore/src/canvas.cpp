#include "canvas.h"

#include <math.h>

namespace cm {

namespace {
// Exact x/255 for 0 <= x <= 65535, without a division.
inline uint32_t div255(uint32_t x) { return (x + 128 + ((x + 128) >> 8)) >> 8; }

inline uint8_t coverage(float v) {
  if (v <= 0.f) return 0;
  if (v >= 1.f) return 255;
  return static_cast<uint8_t>(v * 255.f + 0.5f);
}
}  // namespace

uint16_t blend565(uint16_t dst, uint16_t src, uint8_t a) {
  if (a == 255) return src;
  if (a == 0) return dst;
  const uint32_t ia = 255 - a;
  const uint32_t r = div255(((src >> 11) & 31) * a + ((dst >> 11) & 31) * ia);
  const uint32_t g = div255(((src >> 5) & 63) * a + ((dst >> 5) & 63) * ia);
  const uint32_t b = div255((src & 31) * a + (dst & 31) * ia);
  return static_cast<uint16_t>((r << 11) | (g << 5) | b);
}

void Canvas::blendPixel(int x, int y, uint16_t color, uint8_t alpha) {
  if (x < 0 || y < 0 || x >= w_ || y >= h_ || alpha == 0) return;
  if (flipped_) {
    x = w_ - 1 - x;
    y = h_ - 1 - y;
  }
  uint16_t& p = px_[y * w_ + x];
  p = blend565(p, color, alpha);
}

void Canvas::blendRect(int x, int y, int w, int h, uint16_t color, uint8_t alpha) {
  if (flipped_) {  // a rectangle rotated 180 degrees is still a rectangle
    x = w_ - x - w;
    y = h_ - y - h;
  }
  int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
  int x1 = x + w > w_ ? w_ : x + w, y1 = y + h > h_ ? h_ : y + h;
  for (int yy = y0; yy < y1; ++yy) {
    uint16_t* row = px_ + yy * w_;
    if (alpha == 255) {
      for (int xx = x0; xx < x1; ++xx) row[xx] = color;
    } else {
      for (int xx = x0; xx < x1; ++xx) row[xx] = blend565(row[xx], color, alpha);
    }
  }
}

void Canvas::fillRect(int x, int y, int w, int h, uint16_t color) { blendRect(x, y, w, h, color, 255); }

void Canvas::fillCircle(float cx, float cy, float r, uint16_t color, uint8_t alpha) {
  const int x0 = static_cast<int>(floorf(cx - r - 1)), x1 = static_cast<int>(ceilf(cx + r + 1));
  const int y0 = static_cast<int>(floorf(cy - r - 1)), y1 = static_cast<int>(ceilf(cy + r + 1));
  for (int y = y0; y <= y1; ++y) {
    for (int x = x0; x <= x1; ++x) {
      const float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
      const uint8_t c = coverage(r + 0.5f - sqrtf(dx * dx + dy * dy));
      if (c) blendPixel(x, y, color, static_cast<uint8_t>(div255(c * alpha)));
    }
  }
}

void Canvas::ring(float cx, float cy, float rOuter, float rInner, uint16_t color, uint8_t alpha) {
  const int x0 = static_cast<int>(floorf(cx - rOuter - 1)), x1 = static_cast<int>(ceilf(cx + rOuter + 1));
  const int y0 = static_cast<int>(floorf(cy - rOuter - 1)), y1 = static_cast<int>(ceilf(cy + rOuter + 1));
  for (int y = y0; y <= y1; ++y) {
    for (int x = x0; x <= x1; ++x) {
      const float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
      const float d = sqrtf(dx * dx + dy * dy);
      const uint32_t c = div255(coverage(rOuter + 0.5f - d) * static_cast<uint32_t>(coverage(d - rInner + 0.5f)));
      if (c) blendPixel(x, y, color, static_cast<uint8_t>(div255(c * alpha)));
    }
  }
}

void Canvas::fillRoundRect(int x, int y, int w, int h, int r, uint16_t color, uint8_t alpha) {
  // Middle band and the two side bands, then four antialiased corners.
  blendRect(x, y + r, w, h - 2 * r, color, alpha);
  blendRect(x + r, y, w - 2 * r, r, color, alpha);
  blendRect(x + r, y + h - r, w - 2 * r, r, color, alpha);
  const float rr = static_cast<float>(r);
  for (int cy = 0; cy < r; ++cy) {
    for (int cx = 0; cx < r; ++cx) {
      const float dx = rr - (cx + 0.5f), dy = rr - (cy + 0.5f);
      const uint8_t c = coverage(rr + 0.5f - sqrtf(dx * dx + dy * dy));
      const uint8_t a = static_cast<uint8_t>(div255(c * alpha));
      blendPixel(x + cx, y + cy, color, a);
      blendPixel(x + w - 1 - cx, y + cy, color, a);
      blendPixel(x + cx, y + h - 1 - cy, color, a);
      blendPixel(x + w - 1 - cx, y + h - 1 - cy, color, a);
    }
  }
}

void Canvas::drawSprite(int x, int y, int w, int h, const uint16_t* rgb, const uint8_t* alpha) {
  for (int sy = 0; sy < h; ++sy) {
    for (int sx = 0; sx < w; ++sx) {
      const int i = sy * w + sx;
      if (alpha[i]) blendPixel(x + sx, y + sy, rgb[i], alpha[i]);
    }
  }
}

int Canvas::textWidth(const Font& font, const char* text) const {
  int width = 0;
  for (const char* p = text; *p; ++p) {
    const unsigned char c = static_cast<unsigned char>(*p);
    if (c < font.first || c > font.last) continue;
    width += font.glyphs[c - font.first].advance;
  }
  return width;
}

void Canvas::drawText(const Font& font, int x, int baseline, const char* text, uint16_t color) {
  for (const char* p = text; *p; ++p) {
    const unsigned char c = static_cast<unsigned char>(*p);
    if (c < font.first || c > font.last) continue;
    const Glyph& g = font.glyphs[c - font.first];
    for (int gy = 0; gy < g.h; ++gy) {
      for (int gx = 0; gx < g.w; ++gx) {
        const uint32_t n = g.offset + static_cast<uint32_t>(gy * g.w + gx);
        const uint8_t byte = font.bits[n >> 1];
        const uint8_t a4 = (n & 1) ? (byte & 0x0F) : (byte >> 4);
        if (a4) blendPixel(x + g.xOff + gx, baseline + g.yOff + gy, color, static_cast<uint8_t>(a4 * 17));
      }
    }
    x += g.advance;
  }
}

void Canvas::drawTextCentered(const Font& font, int cx, int baseline, const char* text, uint16_t color) {
  drawText(font, cx - textWidth(font, text) / 2, baseline, text, color);
}

}  // namespace cm
