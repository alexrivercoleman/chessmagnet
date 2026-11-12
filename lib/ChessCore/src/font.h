// Bitmap font format produced by scripts/gen_font.py.
#pragma once
#include <stdint.h>

namespace cm {

struct Glyph {
  uint32_t offset;   // index of first pixel (nibble) in Font::bits
  uint8_t w, h;      // bitmap size
  int8_t xOff;       // bitmap left edge relative to pen position
  int8_t yOff;       // bitmap top edge relative to baseline (negative = above)
  uint8_t advance;   // how far to move the pen afterwards
};

struct Font {
  const uint8_t* bits;   // 4-bit alpha, high nibble first
  const Glyph* glyphs;
  uint8_t first, last;   // character range
  uint8_t ascent, descent;
};

}  // namespace cm

// The generated tables use the unqualified names.
using cm::Font;
using cm::Glyph;
