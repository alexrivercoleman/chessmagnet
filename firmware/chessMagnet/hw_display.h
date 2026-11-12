// Display + backlight for the ESP32-4848S040.
#pragma once
#include <stdint.h>

namespace hw {

class Display {
 public:
  bool begin();                 // panel init; backlight stays off
  uint16_t* backBuffer() { return back_; }  // draw here, then present()
  void present();               // copy the back buffer to the panel
  void setBacklight(bool on);
  void sleepPanel();            // ST7701 sleep-in (screen off saves power)
  void wakePanel();
  void prepareDeepSleep();      // backlight forced off while the chip sleeps

 private:
  uint16_t* front_ = nullptr;   // frame buffer the panel scans out (PSRAM)
  uint16_t* back_ = nullptr;
  bool panelAsleep_ = false;
};

}  // namespace hw
