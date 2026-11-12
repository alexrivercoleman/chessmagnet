// chessMagnet: a two-player chess board for the Guition ESP32-4848S040.
//
// All game, UI and drawing logic lives in lib/ChessCore (tested on the PC).
// This file only connects it to the hardware: touch in, pixels and sound
// out, and power management.
#include <Arduino.h>
#include <ChessCore.h>
#include <esp_bt.h>
#include <esp_sleep.h>
#include <esp_wifi.h>
#include <driver/rtc_io.h>

#include "board_pins.h"
#include "config.h"
#include "hw_audio.h"
#include "hw_display.h"
#include "hw_storage.h"
#include "hw_touch.h"

static hw::Display display;
static hw::Touch touch;
static hw::Audio audio;
static hw::Storage storage;
static cm::App* app = nullptr;
static cm::Canvas* canvas = nullptr;
static cm::Power currentPower = cm::Power::On;

// The radios are never started (we don't call WiFi or Bluetooth APIs), so
// they draw no power. These calls make that explicit and also free the RAM
// reserved for the Bluetooth controller.
static void radiosOff() {
  esp_wifi_stop();    // harmless "not initialised" error if never started
  esp_wifi_deinit();
  esp_bt_controller_disable();
  esp_bt_controller_deinit();
  esp_bt_controller_mem_release(ESP_BT_MODE_BLE);
}

static void drawFrame() {
  app->render(*canvas);
  display.present();
}

static void enterDeepSleep() {
  Serial.println("Idle for an hour: deep sleep. Press BOOT or RST to wake.");
  audio.setActive(false);
  delay(50);
  display.prepareDeepSleep();
#if WAKE_ON_BOOT_BUTTON
  // The BOOT button pulls GPIO0 low. During deep sleep the display isn't
  // using the pin, so it can serve as a wake-up input.
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, 0);
  rtc_gpio_pullup_en(GPIO_NUM_0);
  rtc_gpio_pulldown_dis(GPIO_NUM_0);
#endif
  Serial.flush();
  esp_deep_sleep_start();  // never returns; waking restarts from setup()
}

static void applyPower(cm::Power want) {
  if (want == currentPower) return;
  if (want == cm::Power::DeepSleep) enterDeepSleep();
  if (want == cm::Power::ScreenOff) {
    display.setBacklight(false);
#if PANEL_SLEEP_WHEN_SCREEN_OFF
    display.sleepPanel();
#endif
    audio.setActive(false);
#if LOW_CLOCK_WHEN_SCREEN_OFF
    setCpuFrequencyMhz(80);
#endif
  } else {  // back on
#if LOW_CLOCK_WHEN_SCREEN_OFF
    setCpuFrequencyMhz(240);
#endif
    display.wakePanel();
    drawFrame();  // fresh pixels before the light comes on
    display.setBacklight(true);
    audio.setActive(true);
  }
  currentPower = want;
}

void setup() {
  Serial.begin(115200);
  Serial.printf("\nchessMagnet starting (wake cause %d)\n", static_cast<int>(esp_sleep_get_wakeup_cause()));
  radiosOff();

  if (!display.begin()) Serial.println("Display init FAILED (is PSRAM enabled: OPI PSRAM?)");
  if (!touch.begin()) Serial.println("GT911 touch controller not found on I2C");
  if (!storage.begin()) Serial.println("NVS storage init FAILED");
  if (!audio.begin()) Serial.println("Audio init FAILED");

  static cm::Canvas c(display.backBuffer(), cm::kScreenW, cm::kScreenH);
  canvas = &c;
  static cm::App a(storage, &audio);
  app = &a;
  app->begin(millis());
  drawFrame();
  display.setBacklight(true);
  Serial.printf("Ready. Position: %s\n", app->game().fen().c_str());
}

void loop() {
  const uint32_t now = millis();
  bool pressed;
  int x, y;
  touch.read(pressed, x, y, now);
  app->touch(pressed, x, y, now);

  applyPower(app->power());
  if (currentPower == cm::Power::On && app->needsRedraw()) drawFrame();

  delay(currentPower == cm::Power::On ? POLL_MS_SCREEN_ON : POLL_MS_SCREEN_OFF);
}
