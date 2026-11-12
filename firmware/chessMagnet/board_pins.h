// Pin map for the Guition ESP32-4848S040 (ESP32-S3, 480x480 ST7701 panel).
//
// Each value was cross-checked against independent community sources:
//   [GFX]  Arduino_GFX 1.6.9, examples/PDQgraphicstest/Arduino_GFX_dev_device.h,
//          section ESP32_4848S040_86BOX_GUITION (by the library's author)
//   [RZ]   rzeldent/platformio-espressif32-sunton, esp32-4848S040CIY1.json
//   [EDP]  esp-arduino-libs/ESP32_Display_Panel (Espressif),
//          BOARD_JINGCAI_ESP32_4848S040C_I_Y_3.h
//   [ESPH] esphome/esphome, components/mipi_rgb/models/guition.py
//   [MT]   meshtastic/device-ui, include/graphics/LGFX/LGFX_4848S040.h
//   [YR]   vortigont/yoyoRadio guition_4848S040.hpp; Witaliy76/Yoradio_RGB_Panel
//   [LW]   lcdwizard.com/boards/guition-4848s040-speaker (cites Guition's pin table)
// All sources agree on every value below unless a note says otherwise.
#pragma once

// ---- RGB panel (16-bit RGB565 parallel bus) ---- [GFX][RZ][ESPH][LW]
#define PIN_LCD_DE 18
#define PIN_LCD_VSYNC 17
#define PIN_LCD_HSYNC 16
#define PIN_LCD_PCLK 21
#define PIN_LCD_R0 11
#define PIN_LCD_R1 12
#define PIN_LCD_R2 13
#define PIN_LCD_R3 14
#define PIN_LCD_R4 0   // also the BOOT button / strapping pin
#define PIN_LCD_G0 8
#define PIN_LCD_G1 20
#define PIN_LCD_G2 3
#define PIN_LCD_G3 46
#define PIN_LCD_G4 9
#define PIN_LCD_G5 10
#define PIN_LCD_B0 4
#define PIN_LCD_B1 5
#define PIN_LCD_B2 6
#define PIN_LCD_B3 7
#define PIN_LCD_B4 15

// ---- ST7701 3-wire SPI, used only to send the init sequence ---- [GFX][RZ][EDP][MT]
#define PIN_LCD_CS 39
#define PIN_LCD_SCK 48
#define PIN_LCD_MOSI 47
// Panel reset: not connected [GFX][RZ][EDP][LW]

// ---- Backlight ---- [GFX][RZ][MT][YR][LW], PWM-dimmable per [LW]
#define PIN_BACKLIGHT 38

// ---- GT911 touch (I2C) ---- [RZ][EDP][MT][YR][LW]
#define PIN_TOUCH_SDA 19
#define PIN_TOUCH_SCL 45  // NOTE: homeding.github.io lists SCL=20, but GPIO20 is the
                          // G1 display data line in every other source; 45 is right.
// Touch INT and RST are NOT connected to the ESP32 [RZ][EDP][MT][YR][LW]
// (Tactility's devicetree says the same). So touch is polled over I2C, and a
// touch cannot wake the chip from sleep.
#define GT911_ADDR_A 0x5D  // which address the GT911 uses depends on INT's level at
#define GT911_ADDR_B 0x14  // power-up; INT is floating here, so we probe both.

// ---- Speaker: NS4168 I2S amplifier ---- [YR][LW]
// On relay versions of this board these three pins drive relays instead
// [RZ] (RELAY_1=40, RELAY_2=2, RELAY_3=1); 0-ohm links on the PCB pick which.
#define PIN_I2S_BCLK 1
#define PIN_I2S_LRCK 2
#define PIN_I2S_DOUT 40

// ---- Panel timing ---- [GFX] (RZ agrees; ESPHome-based configs use shorter
// back porches, 20/10, which also work on many units)
#define LCD_PCLK_HZ 12000000
#define LCD_HSYNC_POLARITY 1
#define LCD_HSYNC_FRONT_PORCH 10
#define LCD_HSYNC_PULSE_WIDTH 8
#define LCD_HSYNC_BACK_PORCH 50
#define LCD_VSYNC_POLARITY 1
#define LCD_VSYNC_FRONT_PORCH 10
#define LCD_VSYNC_PULSE_WIDTH 8
#define LCD_VSYNC_BACK_PORCH 20
