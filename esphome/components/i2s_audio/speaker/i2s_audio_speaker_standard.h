#pragma once

#ifdef USE_ESP32

#include "i2s_audio_speaker.h"

namespace esphome::i2s_audio {

/// @brief Standard I2S speaker implementation.
/// Outputs PCM audio data directly to an I2S DAC using the standard I2S protocol.
class I2SAudioSpeaker : public I2SAudioSpeakerBase {
 public:
  void dump_config() override;

  void set_i2s_comm_fmt(std::string mode) { this->i2s_comm_fmt_ = std::move(mode); }

 protected:
  void run_speaker_task_() override;
  esp_err_t start_i2s_driver_(audio::AudioStreamInfo &audio_stream_info) override;

  std::string i2s_comm_fmt_;
};

}  // namespace esphome::i2s_audio

#endif  // USE_ESP32
