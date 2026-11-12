#include "hw_touch.h"

#include <Arduino.h>
#include <Wire.h>

#include "board_pins.h"
#include "config.h"

namespace hw {

namespace {
// GT911 registers (from the Goodix GT911 programming guide)
constexpr uint16_t kRegConfigXMax = 0x8048;  // 4 bytes: x max (LE), y max (LE)
constexpr uint16_t kRegProductId = 0x8140;   // "911"
constexpr uint16_t kRegStatus = 0x814E;      // bit7 = new data, low nibble = points
constexpr uint16_t kRegPoint1 = 0x814F;      // id, xL, xH, yL, yH, sizeL, sizeH, reserved

// If no fresh report arrives for this long, treat the finger as lifted.
constexpr uint32_t kReleaseTimeoutMs = 150;
}  // namespace

bool Touch::begin() {
  Wire.begin(PIN_TOUCH_SDA, PIN_TOUCH_SCL, 400000);
  for (uint8_t a : {static_cast<uint8_t>(GT911_ADDR_A), static_cast<uint8_t>(GT911_ADDR_B)}) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      addr_ = a;
      break;
    }
  }
  if (!addr_) return false;
  uint8_t id[4] = {0};
  readReg(kRegProductId, id, 3);
  uint8_t res[4];
  if (readReg(kRegConfigXMax, res, 4)) {
    const uint16_t mx = res[0] | (res[1] << 8), my = res[2] | (res[3] << 8);
    if (mx > 0 && my > 0 && mx < 4096 && my < 4096) {
      maxX_ = mx;
      maxY_ = my;
    }
  }
  Serial.printf("GT911 at 0x%02X, id \"%s\", resolution %ux%u\n", addr_, reinterpret_cast<char*>(id), maxX_, maxY_);
  writeReg(kRegStatus, 0);
  return true;
}

bool Touch::readReg(uint16_t reg, uint8_t* buf, uint8_t len) {
  Wire.beginTransmission(addr_);
  Wire.write(reg >> 8);
  Wire.write(reg & 0xFF);
  if (Wire.endTransmission() != 0) return false;
  if (Wire.requestFrom(addr_, len) != len) return false;
  for (uint8_t i = 0; i < len; ++i) buf[i] = Wire.read();
  return true;
}

bool Touch::writeReg(uint16_t reg, uint8_t value) {
  Wire.beginTransmission(addr_);
  Wire.write(reg >> 8);
  Wire.write(reg & 0xFF);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

void Touch::read(bool& pressed, int& x, int& y, uint32_t nowMs) {
  uint8_t status = 0;
  if (addr_ && readReg(kRegStatus, &status, 1) && (status & 0x80)) {
    lastReport_ = nowMs;
    const uint8_t points = status & 0x0F;
    uint8_t p[8];
    if (points > 0 && points <= 5 && readReg(kRegPoint1, p, sizeof(p))) {
      int rx = p[1] | (p[2] << 8);
      int ry = p[3] | (p[4] << 8);
      // Scale to panel pixels in case the controller is configured differently.
      rx = rx * 480 / maxX_;
      ry = ry * 480 / maxY_;
#if TOUCH_SWAP_XY
      const int t = rx;
      rx = ry;
      ry = t;
#endif
#if TOUCH_INVERT_X
      rx = 479 - rx;
#endif
#if TOUCH_INVERT_Y
      ry = 479 - ry;
#endif
      x_ = rx < 0 ? 0 : (rx > 479 ? 479 : rx);
      y_ = ry < 0 ? 0 : (ry > 479 ? 479 : ry);
      pressed_ = true;
    } else {
      pressed_ = false;
    }
    writeReg(kRegStatus, 0);  // tell the GT911 we've read this report
#if TOUCH_DEBUG
    Serial.printf("touch %s %d,%d\n", pressed_ ? "down" : "up", x_, y_);
#endif
  } else if (pressed_ && nowMs - lastReport_ > kReleaseTimeoutMs) {
    pressed_ = false;
  }
  pressed = pressed_;
  x = x_;
  y = y_;
}

}  // namespace hw
