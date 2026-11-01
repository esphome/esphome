#pragma once

#include "esphome/components/audio/audio.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace esphome::i2s_audio {

/// Removes DC from mono or stereo PCM, retaining separate channel histories between buffers.
class DCOffsetFilter {
 public:
  void reset(uint32_t sample_rate) {
    // A 20 Hz pole gives the same settling time at every sample rate. Normalize the
    // feedforward coefficient to keep unity gain at Nyquist: b = (1 + a) / 2.
    const double pole = std::exp(-2.0 * 3.14159265358979323846 * 20.0 / sample_rate);
    this->feedforward_ = static_cast<int32_t>(std::lround((1.0 + pole) * 0.5 * COEFFICIENT_SCALE));
    this->feedback_ = static_cast<int32_t>(2 * static_cast<int64_t>(this->feedforward_) - COEFFICIENT_SCALE);
    this->states_ = {};
  }

  // The I2S driver supplies complete mono or stereo frames with the sample rate used by reset().
  void process(uint8_t *data, size_t size, const audio::AudioStreamInfo &stream_info) {
    const size_t bytes_per_sample = stream_info.samples_to_bytes(1);
    const size_t channels = stream_info.get_channels();
    const size_t frames = stream_info.bytes_to_frames(size);
    for (size_t frame = 0; frame < frames; ++frame) {
      for (size_t channel = 0; channel < channels; ++channel) {
        auto &state = this->states_[channel];
        const int32_t input = audio::unpack_audio_sample_to_q31(data, bytes_per_sample);
        // Q30 coefficients leave room for full-scale steps in the 64-bit products.
        // Keep the unclipped history so clipping a transient does not change the filter's decay.
        const int64_t difference = static_cast<int64_t>(input) - state.input;
        const int64_t output = ((this->feedforward_ * difference) >> 30) + ((this->feedback_ * state.output) >> 30);
        state.input = input;
        state.output = output;
        const int32_t sample = static_cast<int32_t>(
            std::clamp<int64_t>(output, std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::max()));
        audio::pack_q31_as_audio_sample(sample, data, bytes_per_sample);
        data += bytes_per_sample;
      }
    }
  }

 protected:
  static constexpr int64_t COEFFICIENT_SCALE = int64_t{1} << 30;
  struct ChannelState {
    int32_t input{0};
    int64_t output{0};
  };
  std::array<ChannelState, 2> states_{};
  int32_t feedforward_{0};
  int32_t feedback_{0};
};

}  // namespace esphome::i2s_audio
