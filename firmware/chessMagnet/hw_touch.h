// Minimal polled driver for the GT911 touch controller.
//
// Why our own ~100 lines instead of a library: on this board the GT911's INT
// and RST pins aren't wired to the ESP32, which many GT911 libraries expect.
// We only need "is a finger down, and where", polled over I2C.
#pragma once
#include <stdint.h>

namespace hw {

class Touch {
 public:
  bool begin();
  // Returns the current state. x/y are panel pixels (0..479), already
  // corrected with the TOUCH_* settings in config.h.
  void read(bool& pressed, int& x, int& y, uint32_t nowMs);
  uint8_t address() const { return addr_; }

 private:
  bool readReg(uint16_t reg, uint8_t* buf, uint8_t len);
  bool writeReg(uint16_t reg, uint8_t value);

  uint8_t addr_ = 0;
  uint16_t maxX_ = 480, maxY_ = 480;
  bool pressed_ = false;
  int x_ = 0, y_ = 0;
  uint32_t lastReport_ = 0;
};

}  // namespace hw
