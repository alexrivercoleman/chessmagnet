#include "hw_display.h"

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <driver/gpio.h>
#include <esp_heap_caps.h>

#include "board_pins.h"
#include "config.h"

namespace hw {

namespace {
constexpr int kW = 480, kH = 480;
constexpr size_t kBytes = kW * kH * sizeof(uint16_t);

// The panel has two connections:
//  * a slow 3-wire SPI link, used once at start-up to send the ST7701 its
//    configuration ("init sequence"), and later for sleep/wake commands;
//  * a fast 16-bit parallel RGB bus that continuously streams the frame
//    buffer from PSRAM to the glass (done by the ESP32-S3's LCD hardware).
Arduino_DataBus* g_bus = nullptr;
Arduino_ESP32RGBPanel* g_rgb = nullptr;
Arduino_RGB_Display* g_gfx = nullptr;

void sendCommand(uint8_t cmd) {
  g_bus->beginWrite();
  g_bus->writeCommand(cmd);
  g_bus->endWrite();
}
}  // namespace

bool Display::begin() {
  // A deep-sleep "hold" on the backlight pin survives the wake-up; release it.
  gpio_hold_dis(static_cast<gpio_num_t>(PIN_BACKLIGHT));
  gpio_deep_sleep_hold_dis();
  ledcAttach(PIN_BACKLIGHT, 1000 /* Hz */, 8 /* bits */);
  setBacklight(false);  // keep dark until the first frame is ready

  g_bus = new Arduino_SWSPI(GFX_NOT_DEFINED /* DC */, PIN_LCD_CS, PIN_LCD_SCK, PIN_LCD_MOSI,
                            GFX_NOT_DEFINED /* MISO */);
  g_rgb = new Arduino_ESP32RGBPanel(
      PIN_LCD_DE, PIN_LCD_VSYNC, PIN_LCD_HSYNC, PIN_LCD_PCLK,
      PIN_LCD_R0, PIN_LCD_R1, PIN_LCD_R2, PIN_LCD_R3, PIN_LCD_R4,
      PIN_LCD_G0, PIN_LCD_G1, PIN_LCD_G2, PIN_LCD_G3, PIN_LCD_G4, PIN_LCD_G5,
      PIN_LCD_B0, PIN_LCD_B1, PIN_LCD_B2, PIN_LCD_B3, PIN_LCD_B4,
      LCD_HSYNC_POLARITY, LCD_HSYNC_FRONT_PORCH, LCD_HSYNC_PULSE_WIDTH, LCD_HSYNC_BACK_PORCH,
      LCD_VSYNC_POLARITY, LCD_VSYNC_FRONT_PORCH, LCD_VSYNC_PULSE_WIDTH, LCD_VSYNC_BACK_PORCH,
      0 /* pclk_active_neg */, LCD_PCLK_HZ, false /* useBigEndian */);
  // rotation 0: we write raw pixels ourselves, rotating in software when needed.
  // auto_flush false: we decide when the finished frame is pushed out.
  g_gfx = new Arduino_RGB_Display(kW, kH, g_rgb, 0 /* rotation */, false /* auto_flush */, g_bus,
                                  GFX_NOT_DEFINED /* RST */, st7701_type9_init_operations,
                                  sizeof(st7701_type9_init_operations));
  if (!g_gfx->begin()) return false;
  front_ = g_gfx->getFramebuffer();
  // A second buffer so a half-drawn frame is never visible. PSRAM has room.
  back_ = static_cast<uint16_t*>(heap_caps_malloc(kBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!front_ || !back_) return false;
  memset(back_, 0, kBytes);
  present();
  return true;
}

void Display::present() {
  memcpy(front_, back_, kBytes);
  // The CPU writes through a cache; flush it so the LCD hardware (which reads
  // PSRAM directly) sees the new pixels.
  g_gfx->flush(true);
}

void Display::setBacklight(bool on) { ledcWrite(PIN_BACKLIGHT, on ? BACKLIGHT_LEVEL : 0); }

void Display::sleepPanel() {
  if (panelAsleep_) return;
  sendCommand(0x28);  // DISPOFF
  sendCommand(0x10);  // SLPIN
  delay(5);
  panelAsleep_ = true;
}

void Display::wakePanel() {
  if (!panelAsleep_) return;
  sendCommand(0x11);  // SLPOUT
  delay(120);         // ST7701 datasheet: wait 120 ms after sleep-out
  sendCommand(0x29);  // DISPON
  panelAsleep_ = false;
}

void Display::prepareDeepSleep() {
  setBacklight(false);
  sleepPanel();
  // Drive the backlight pin low and "hold" it, so it can't float (and light
  // up the backlight) while the chip is in deep sleep.
  ledcDetach(PIN_BACKLIGHT);
  pinMode(PIN_BACKLIGHT, OUTPUT);
  digitalWrite(PIN_BACKLIGHT, LOW);
  gpio_hold_en(static_cast<gpio_num_t>(PIN_BACKLIGHT));
  gpio_deep_sleep_hold_en();
}

}  // namespace hw
