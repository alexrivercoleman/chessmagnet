// Screen layout and colours, in logical coordinates (player to move at the
// bottom). Change the look of the app here.
#pragma once
#include "canvas.h"

namespace cm {

struct Rect {
  int x, y, w, h;
  bool contains(int px, int py) const { return px >= x && py >= y && px < x + w && py < y + h; }
};

constexpr int kSquare = 60;

// Board
constexpr uint16_t kLightSquare = rgb565(238, 238, 210);
constexpr uint16_t kDarkSquare = rgb565(118, 150, 86);
constexpr uint16_t kHighlight = rgb565(255, 230, 60);   // last move + selection
constexpr uint8_t kLastMoveAlpha = 105;
constexpr uint8_t kSelectedAlpha = 175;
constexpr uint16_t kHint = rgb565(20, 40, 20);          // legal-move dots/rings
constexpr uint8_t kHintAlpha = 80;
constexpr uint16_t kCheck = rgb565(230, 30, 30);

// Panels and text
constexpr uint16_t kPanel = rgb565(38, 36, 33);
constexpr uint16_t kText = rgb565(255, 255, 255);
constexpr uint16_t kTextDim = rgb565(190, 186, 180);
constexpr uint16_t kButton = rgb565(90, 88, 84);
constexpr uint16_t kButtonGo = rgb565(98, 153, 36);
constexpr uint16_t kPromoBg = rgb565(250, 250, 250);

// New-game confirmation dialog
constexpr Rect kConfirmPanel{40, 140, 400, 200};
constexpr Rect kConfirmNo{70, 240, 160, 64};
constexpr Rect kConfirmYes{250, 240, 160, 64};

}  // namespace cm
