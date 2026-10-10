#pragma once

#ifdef USE_ESP32

#include "esphome/components/audio/audio.h"
#include "esphome/components/microphone/microphone.h"
#include "esphome/components/microphone/microphone_source.h"

#include "esphome/core/component.h"

#include <resampler.h>  // esp-audio-libs

#include <memory>
#include <vector>

namespace esphome::resampler {

/// @brief Microphone that converts the audio of a source microphone to a different sample rate.
/// The bits per sample and channels are selected by the source's ``MicrophoneSource``; only the sample rate changes.
/// Resampling runs in the source microphone's data callback, so it needs no task or ring buffer of its own.
class ResamplerMicrophone final : public Component, public microphone::Microphone {
 public:
  explicit ResamplerMicrophone(microphone::MicrophoneSource *source) : source_(source) {}

  void setup() override;
  void loop() override;
  void dump_config() override;

  void start() override;
  void stop() override;

  void set_target_sample_rate(uint32_t target_sample_rate) { this->target_sample_rate_ = target_sample_rate; }
  void set_filters(uint16_t filters) { this->filters_ = filters; }
  void set_taps(uint16_t taps) { this->taps_ = taps; }

 protected:
  /// @brief Sets up the resampler for the given input format. No resampler is needed if the sample rates match.
  /// @return false if the resampler failed to allocate; the audio is then dropped
  bool init_resampler_(const audio::AudioStreamInfo &input_stream_info);

  /// @brief Resamples a chunk of source audio and passes it to the data callbacks. Source microphone task only.
  void process_audio_(const std::vector<uint8_t> &data);

  microphone::MicrophoneSource *source_;
  std::unique_ptr<esp_audio_libs::resampler::Resampler> resampler_;
  // Reused for every chunk so resampling does not allocate
  std::vector<uint8_t> output_buffer_;

  // Format the resampler is set up for
  audio::AudioStreamInfo input_stream_info_;

  uint32_t target_sample_rate_;
  uint16_t taps_;
  uint16_t filters_;

  uint8_t active_listeners_{0};
  bool resampler_ready_{false};
};

}  // namespace esphome::resampler

#endif  // USE_ESP32
