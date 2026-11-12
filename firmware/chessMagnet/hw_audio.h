// Speaker output through the NS4168 I2S amplifier.
//
// Sounds play on a background task so a move never waits for audio.
#pragma once
#include <ChessCore.h>

namespace hw {

class Audio : public cm::AudioOut {
 public:
  bool begin();
  void play(cm::Sound s) override;
  void setActive(bool on);  // stop the I2S clock while the screen is off
  bool hasCustomWin() const { return winBytes_ > 0; }

 private:
  static void taskEntry(void* self);
  void taskLoop();
  void findWinWav();
  void writeMono(const int16_t* samples, size_t count);
  void playCustomWin();
  bool startI2S(uint32_t rate);

  void* queue_ = nullptr;
  bool active_ = false;
  uint32_t rate_ = 0;
  std::vector<int16_t> clips_[4];
  // Custom win.wav in the "audio" flash partition
  const void* winPart_ = nullptr;
  uint32_t winOffset_ = 0, winBytes_ = 0, winRate_ = 0;
  uint16_t winChannels_ = 1;
};

}  // namespace hw
