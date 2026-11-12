// Settings you may want to tweak, especially during first hardware bring-up.
#pragma once

// ---------------------------------------------------------------- touch
// If taps land on the wrong square, fix the mapping here (see README,
// "First power-on checklist"). Most sources say no mirroring is needed.
#define TOUCH_SWAP_XY 0
#define TOUCH_INVERT_X 0
#define TOUCH_INVERT_Y 0
// Print every touch to the serial monitor (115200 baud).
#define TOUCH_DEBUG 0

// ---------------------------------------------------------------- display
// Backlight brightness, 0..256 (256 = fully on).
#define BACKLIGHT_LEVEL 256

// ---------------------------------------------------------------- sound
// OFF until you've checked your board. GPIO 1/2/40 go either to the speaker
// amplifier or to relays, set by 0-ohm links on the PCB. On a relay board,
// audio on these pins would chatter the relay. Set to 1 only if your board has
// the NS4168 amplifier fitted and linked (see README, "Sound").
#define AUDIO_ENABLED 0
// Speaker volume, 0..256.
#define AUDIO_VOLUME 160

// ---------------------------------------------------------------- power
// While the screen is off: put the ST7701 panel to sleep and drop the CPU to
// 80 MHz. Both save battery; turn off if the screen misbehaves after waking.
#define PANEL_SLEEP_WHEN_SCREEN_OFF 1
#define LOW_CLOCK_WHEN_SCREEN_OFF 1
// Touch polling interval in milliseconds.
#define POLL_MS_SCREEN_ON 15
#define POLL_MS_SCREEN_OFF 100
// After deep sleep, the BOOT button (GPIO0) wakes the board. RST always works.
#define WAKE_ON_BOOT_BUTTON 1
// The idle timeouts (3 min screen off, 1 h deep sleep) live in AppConfig in
// lib/ChessCore/src/app.h.
