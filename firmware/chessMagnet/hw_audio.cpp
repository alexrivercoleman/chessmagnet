#include "hw_audio.h"

#include <Arduino.h>
#include <ESP_I2S.h>
#include <esp_partition.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "board_pins.h"
#include "config.h"

namespace hw {

namespace {
I2SClass g_i2s;

// Messages to the audio task. Values below 0x80 are cm::Sound ids.
constexpr uint8_t kMsgActivate = 0x80;
constexpr uint8_t kMsgDeactivate = 0x81;

uint32_t le32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | (static_cast<uint32_t>(p[3]) << 24); }
uint16_t le16(const uint8_t* p) { return p[0] | (p[1] << 8); }
}  // namespace

bool Audio::begin() {
#if !AUDIO_ENABLED
  // Leave GPIO 1/2/40 untouched (they may drive relays on this board).
  Serial.println("Audio disabled (AUDIO_ENABLED 0 in config.h)");
  return true;
#endif
  // Synthesise the built-in sounds once at start-up (a few hundred KB of
  // PSRAM at most) so playing them later is instant.
  for (int i = 0; i < 4; ++i) cm::synth::render(static_cast<cm::Sound>(i), clips_[i]);
  findWinWav();
  g_i2s.setPins(PIN_I2S_BCLK, PIN_I2S_LRCK, PIN_I2S_DOUT);
  queue_ = xQueueCreate(8, sizeof(uint8_t));
  if (!queue_) return false;
  xTaskCreatePinnedToCore(taskEntry, "audio", 4096, this, 2, nullptr, 0);
  setActive(true);
  return true;
}

void Audio::play(cm::Sound s) {
  if (!queue_) return;  // audio disabled or not started
  const uint8_t msg = static_cast<uint8_t>(s);
  xQueueSend(static_cast<QueueHandle_t>(queue_), &msg, 0);  // never blocks the UI
}

void Audio::setActive(bool on) {
  if (!queue_) return;
  const uint8_t msg = on ? kMsgActivate : kMsgDeactivate;
  xQueueSend(static_cast<QueueHandle_t>(queue_), &msg, pdMS_TO_TICKS(50));
}

void Audio::taskEntry(void* self) { static_cast<Audio*>(self)->taskLoop(); }

bool Audio::startI2S(uint32_t rate) {
  if (active_ && rate == rate_) return true;
  if (active_) g_i2s.end();
  // Stereo with the same sample on both channels: the NS4168 picks left or
  // right depending on how its CTRL pin is wired, and we don't know which.
  active_ = g_i2s.begin(I2S_MODE_STD, rate, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);
  rate_ = active_ ? rate : 0;
  if (!active_) Serial.println("I2S start failed");
  return active_;
}

void Audio::taskLoop() {
  bool wanted = false;
  for (;;) {
    uint8_t msg;
    if (xQueueReceive(static_cast<QueueHandle_t>(queue_), &msg, portMAX_DELAY) != pdTRUE) continue;
    if (msg == kMsgActivate) {
      wanted = true;
      startI2S(cm::synth::kSampleRate);
      continue;
    }
    if (msg == kMsgDeactivate) {
      wanted = false;
      if (active_) g_i2s.end();
      active_ = false;
      continue;
    }
    if (!wanted || msg >= 4) continue;
    const cm::Sound s = static_cast<cm::Sound>(msg);
    if (s == cm::Sound::Win && hasCustomWin()) {
      playCustomWin();
    } else if (startI2S(cm::synth::kSampleRate)) {
      writeMono(clips_[msg].data(), clips_[msg].size());
    }
    // A little silence so the amplifier doesn't repeat stale samples.
    static const int16_t zeros[320] = {0};
    writeMono(zeros, 320);
  }
}

void Audio::writeMono(const int16_t* samples, size_t count) {
  if (!active_) return;
  int16_t stereo[2 * 256];
  while (count > 0) {
    const size_t n = count > 256 ? 256 : count;
    for (size_t i = 0; i < n; ++i) {
      const int16_t v = static_cast<int16_t>((static_cast<int32_t>(samples[i]) * AUDIO_VOLUME) >> 8);
      stereo[2 * i] = stereo[2 * i + 1] = v;
    }
    g_i2s.write(reinterpret_cast<const uint8_t*>(stereo), n * 2 * sizeof(int16_t));
    samples += n;
    count -= n;
  }
}

// Looks for a 16-bit PCM WAV file written raw at the start of the "audio"
// partition (see scripts/flash_win_sound.ps1).
void Audio::findWinWav() {
  const esp_partition_t* part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "audio");
  if (!part) return;
  uint8_t hdr[256];
  if (esp_partition_read(part, 0, hdr, sizeof(hdr)) != ESP_OK) return;
  if (memcmp(hdr, "RIFF", 4) != 0 || memcmp(hdr + 8, "WAVE", 4) != 0) {
    Serial.println("No win.wav flashed; using the built-in fanfare");
    return;
  }
  uint32_t pos = 12, rate = 0, dataOff = 0, dataLen = 0;
  uint16_t fmt = 0, channels = 0, bits = 0;
  while (pos + 8 <= sizeof(hdr)) {
    const uint32_t size = le32(hdr + pos + 4);
    if (memcmp(hdr + pos, "fmt ", 4) == 0 && pos + 24 <= sizeof(hdr)) {
      fmt = le16(hdr + pos + 8);
      channels = le16(hdr + pos + 10);
      rate = le32(hdr + pos + 12);
      bits = le16(hdr + pos + 22);
    } else if (memcmp(hdr + pos, "data", 4) == 0) {
      dataOff = pos + 8;
      dataLen = size;
      break;
    }
    pos += 8 + size + (size & 1);
  }
  if (fmt != 1 || bits != 16 || (channels != 1 && channels != 2) || rate < 8000 || rate > 48000 || !dataOff) {
    Serial.println("audio partition: unsupported WAV (need 16-bit PCM, mono or stereo)");
    return;
  }
  if (dataOff + dataLen > part->size) dataLen = part->size - dataOff;
  winPart_ = part;
  winOffset_ = dataOff;
  winBytes_ = dataLen;
  winRate_ = rate;
  winChannels_ = channels;
  Serial.printf("win.wav: %u Hz, %u ch, %.1f s\n", rate, channels, dataLen / (2.0f * channels * rate));
}

void Audio::playCustomWin() {
  if (!startI2S(winRate_)) return;
  const esp_partition_t* part = static_cast<const esp_partition_t*>(winPart_);
  int16_t buf[512];
  int16_t mono[256];
  uint32_t done = 0;
  while (done < winBytes_) {
    uint32_t n = winBytes_ - done;
    if (n > sizeof(buf)) n = sizeof(buf);
    n &= ~3u;  // whole stereo frames
    if (!n || esp_partition_read(part, winOffset_ + done, buf, n) != ESP_OK) break;
    if (winChannels_ == 2) {
      const size_t frames = n / 4;
      for (size_t i = 0; i < frames; ++i) mono[i] = static_cast<int16_t>((buf[2 * i] + buf[2 * i + 1]) / 2);
      writeMono(mono, frames);
    } else {
      writeMono(buf, n / 2);
    }
    done += n;
  }
  startI2S(cm::synth::kSampleRate);  // back to the rate of the built-in sounds
}

}  // namespace hw
