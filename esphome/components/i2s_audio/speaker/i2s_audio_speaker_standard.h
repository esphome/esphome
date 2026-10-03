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
class I2SAudioSpeaker final : public I2SAudioSpeakerBase {
 public:
  void dump_config() override;

  void set_i2s_comm_fmt(I2SCommFmt fmt) { this->i2s_comm_fmt_ = fmt; }

#ifdef USE_I2S_AUDIO_FULL_DUPLEX
  bool build_full_duplex_config(i2s_chan_config_t &chan_cfg, i2s_std_config_t &std_cfg) override;
#endif

 protected:
  void run_speaker_task() override;
  esp_err_t start_i2s_driver(audio::AudioStreamInfo &audio_stream_info) override;

  /// @brief Builds the channel and standard mode configuration for the given output format.
  /// @param output_stream_info Format clocked out of the I2S peripheral
  void build_i2s_config_(const audio::AudioStreamInfo &output_stream_info, i2s_chan_config_t &chan_cfg,
                         i2s_std_config_t &std_cfg) const;

#ifdef USE_I2S_AUDIO_FULL_DUPLEX
  /// @brief The output format of a full duplex bus, fixed by the configuration since the channel is set up once.
  audio::AudioStreamInfo full_duplex_stream_info_() const;
#endif

  I2SCommFmt i2s_comm_fmt_{I2SCommFmt::STANDARD};
};

}  // namespace esphome::i2s_audio

#endif  // USE_ESP32
