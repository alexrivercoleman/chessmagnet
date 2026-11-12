# chessMagnet

A fridge-magnet chess board: a 4" square touchscreen on which two people play
chess. After every move the picture turns 180° so the other player can spin
the device round and play. No WiFi, no Bluetooth.

Hardware: Guition ESP32-4848S040 (ESP32-S3 N16R8, 480×480 ST7701 IPS panel,
GT911 touch, NS4168 I²S speaker amp), 3.7 V LiPo on the BAT connector.

## How to play

- Tap a piece to select it. Dots show quiet moves, rings show captures.
  Tap a highlighted square to move.
- Castle by moving the king two squares.
- On promotion, a picker appears (queen, rook, bishop, knight). Tap elsewhere to cancel.
- The last move is highlighted, and a king in check glows red.
- Checkmate, stalemate and automatic draws (insufficient material, 50-move
  rule, threefold repetition) show a result banner, drawn facing both players.
  Tap to hide or show it.
- **Hold anywhere for 2 seconds** to start a new game. A bar shows progress,
  then a confirmation asks before anything is lost.
- The game is saved to flash after every move and survives power loss.
- After 3 minutes without a touch the screen turns off; touch to wake. That
  first touch only wakes the screen.
- After 1 hour idle the board goes into deep sleep. Press **BOOT** (or RST) to
  wake it; the game picks up where it was.

## Project layout

```
lib/ChessCore/          hardware-independent code (game, UI, drawing, sounds)
  src/chess/            Disservin's chess-library (vendored, see VERSION.txt)
  src/gen/              generated piece images + fonts (C arrays)
firmware/chessMagnet/   ESP32 sketch: display, touch, audio, storage, power
  board_pins.h          pin map, with a source for every value
  config.h              settings for bring-up (touch mapping, volume, power)
  partitions.csv        flash layout (4 MB app, 2 MB audio, NVS)
desktop/                PC programs: perft test, UI tests, PNG/WAV preview
scripts/                setup, build, asset generation, sound flashing
```

The game logic never touches hardware. The firmware passes touch samples
and the time to `cm::App`, which draws into a 480×480 RGB565 buffer. On the
board that buffer goes to the panel. On the PC the same pixels are saved as
PNG files.

## Building

```powershell
powershell -ExecutionPolicy Bypass -File scripts\setup.ps1           # once
powershell -ExecutionPolicy Bypass -File scripts\build_desktop.ps1   # tests + preview
powershell -ExecutionPolicy Bypass -File scripts\build_firmware.ps1  # ESP32 build
```

The preview images and sounds are written to `build\preview\`.

Board settings used by the firmware build: ESP32S3 Dev Module, 16 MB flash,
OPI PSRAM, custom partition table (`firmware/chessMagnet/partitions.csv`).

Uploading (once the board is connected; find the COM port in Device Manager):

```powershell
$env:ARDUINO_DIRECTORIES_USER = "$PWD\.arduino"
tools\arduino-cli.exe upload --fqbn esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,PartitionScheme=custom -p COM5 --input-dir build\firmware firmware\chessMagnet
tools\arduino-cli.exe monitor -p COM5 -c baudrate=115200
```

### Win sound (optional)

Convert your clip (any format ffmpeg reads, under a minute long) into
`data\win.wav`, which git ignores. Then flash it once; normal firmware uploads
leave it alone:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\make_win_sound.ps1 -In "$HOME\Downloads\clip.mp3"
powershell -ExecutionPolicy Bypass -File scripts\flash_win_sound.ps1 -Wav data\win.wav -Port COM5
```

The conversion makes it 16 kHz mono 16-bit, raises the peak to −1 dBFS,
adds a 0.3 s fade-out and strips metadata.

Without it, a short built-in fanfare plays on checkmate.

## First power-on checklist

These depend on the exact hardware and can't be verified before it arrives:

1. **Serial log.** It should show `GT911 at 0x5D` (or `0x14`) and `Ready`.
2. **Colours.** The light squares are cream and the dark squares green. If red
   and blue look swapped, tell me and I'll adjust the panel setup.
3. **Touch alignment.** Set `TOUCH_DEBUG 1` in `config.h` and tap the corners.
   Top-left should read near `0,0` and bottom-right near `479,479`. If they
   don't, change `TOUCH_SWAP_XY` / `TOUCH_INVERT_X` / `TOUCH_INVERT_Y`.
4. **Screen off / wake.** Wait 3 minutes, then touch. If the picture is
   garbled after waking, set `PANEL_SLEEP_WHEN_SCREEN_OFF 0`.
5. **Deep sleep.** Shorten `deepSleepMs` in `lib/ChessCore/src/app.h` to
   test, then press BOOT. If pressing BOOT leaves the screen dark (the chip
   started in download mode), press RST. If that happens every time, set
   `WAKE_ON_BOOT_BUTTON 0` and use RST to wake instead.
6. **Sound.** This is off by default (`AUDIO_ENABLED 0` in `config.h`). GPIO 1, 2
   and 40 go either to the NS4168 speaker amplifier or to relays, depending on
   three 0-ohm links on the PCB (speaker: R21–R23; relay: R25–R27, according
   to lcdwizard.com). Sending audio to a relay board would make the relay
   chatter. Before enabling sound, look at the board. Is there a relay (a
   cube-shaped box), and is the small amplifier chip next to the speaker
   connector fitted? Turn sound on only if the amplifier is fitted and its
   links are in place.

## Credits and licences

- **Chess pieces:** "Cburnett" set by
  [Cburnett](https://commons.wikimedia.org/wiki/User:Cburnett), from
  [Wikimedia Commons](https://commons.wikimedia.org/wiki/Category:SVG_chess_pieces).
  The author offers it under GFDL, BSD or GPL; this project uses it under the
  **BSD 3-clause licence**. The pieces were rendered by Commons and
  downsampled to 60×60 by `scripts/gen_pieces.py`. Source pages are listed
  in `lib/ChessCore/src/gen/pieces.h`.

  > Copyright (c) Cburnett. All rights reserved.
  >
  > Redistribution and use in source and binary forms, with or without
  > modification, are permitted provided that the following conditions are met:
  > 1. Redistributions of source code must retain the above copyright notice,
  >    this list of conditions and the following disclaimer.
  > 2. Redistributions in binary form must reproduce the above copyright
  >    notice, this list of conditions and the following disclaimer in the
  >    documentation and/or other materials provided with the distribution.
  > 3. Neither the name of the copyright holder nor the names of its
  >    contributors may be used to endorse or promote products derived from
  >    this software without specific prior written permission.
  >
  > THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  > AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  > IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  > ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
  > LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  > CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  > SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  > INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
  > CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  > ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  > POSSIBILITY OF SUCH DAMAGE.

- **Chess rules:** [chess-library](https://github.com/Disservin/chess-library)
  by Disservin, MIT licence (`lib/ChessCore/src/chess/LICENSE`). It includes
  one small local patch that moves the attack tables to PSRAM; see `VERSION.txt`.
- **Font:** Fira Sans Bold, © 2012-2015 The Mozilla Foundation and
  Telefonica S.A., SIL Open Font License 1.1 (`assets/fonts/OFL.txt`).
- **Display driver:** [Arduino_GFX](https://github.com/moononournation/Arduino_GFX)
  1.6.9 by moononournation (BSD licence), including its ST7701 init sequence
  for this board.
- The sound effects and fanfare are original and synthesised in code
  (`lib/ChessCore/src/sound.cpp`).
