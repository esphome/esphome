#pragma once

#ifdef USE_ESP32

#include "i2s_audio_speaker.h"

namespace esphome::i2s_audio {

enum class I2SCommFmt : uint8_t {
  STANDARD,  // Philips / I2S standard
  PCM,       // PCM short
  MSB,       // MSB / left-justified
};

/// @brief Standard I2S speaker implementation.
/// Outputs PCM audio data directly to an I2S DAC using the standard I2S protocol.
class I2SAudioSpeaker : public I2SAudioSpeakerBase {
 public:
  void dump_config() override;

  void set_i2s_comm_fmt(I2SCommFmt fmt) { this->i2s_comm_fmt_ = fmt; }

 protected:
  void run_speaker_task() override;
  esp_err_t start_i2s_driver(audio::AudioStreamInfo &audio_stream_info) override;

  I2SCommFmt i2s_comm_fmt_{I2SCommFmt::STANDARD};
};

}  // namespace esphome::i2s_audio

#endif  // USE_ESP32
