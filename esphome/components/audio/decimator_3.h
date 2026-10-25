#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

#if defined(USE_DSP_DECIMATOR) && defined(USE_ESP_IDF)
#include <cmath>
#include <dsps_fir.h>
#endif

namespace esphome::audio {

#if defined(USE_DSP_DECIMATOR) && defined(USE_ESP_IDF)
// ESP-DSP's Xtensa FPU FIR is faster than the scalar Q14 loop. Its floating
// result is saturated here; the library's s16 FIR wraps on full-scale input.
class DspDecimator3 {
 public:
  void request_reset() { this->reset_pending_.store(true, std::memory_order_relaxed); }

  bool reset() {
    this->coefficients_[0] = 0.0f;  // 64th tap required by the DSP FIR
    for (size_t i = 0; i < 63; ++i)
      this->coefficients_[i + 1] = static_cast<float>(COEFFICIENTS[62 - i]) / 16384.0f;
    this->pending_count_ = 0;
    return dsps_fird_init_f32(&this->fir_, this->coefficients_.data(), this->delay_.data(), 64, 3) == ESP_OK;
  }

  bool process(const std::vector<uint8_t> &input, std::vector<uint8_t> &output) {
    return this->process(input.data(), input.size(), output);
  }

  bool process(const uint8_t *input, size_t input_bytes, std::vector<uint8_t> &output) {
    output.resize(((input_bytes / sizeof(int16_t) + 2) / 3) * sizeof(int16_t));
    size_t generated = 0;
    if (!this->process_impl_(
            input_bytes / sizeof(int16_t), [&](size_t i) { return decode(input, i); },
            [&](size_t i, int16_t sample) {
              output[2 * i] = static_cast<uint8_t>(sample);
              output[2 * i + 1] = static_cast<uint8_t>(sample >> 8);
            },
            generated)) {
      output.clear();
      return false;
    }
    output.resize(generated * sizeof(int16_t));
    return true;
  }

  bool process(const int16_t *input, size_t input_samples, int16_t *output, size_t output_capacity, size_t &generated) {
    if ((this->pending_count_ + input_samples) / 3 > output_capacity)
      return false;
    return this->process_impl_(
        input_samples, [&](size_t i) { return input[i]; }, [&](size_t i, int16_t sample) { output[i] = sample; },
        generated);
  }

 private:
  template<typename ReadSample, typename WriteSample>
  bool process_impl_(size_t input_samples, ReadSample read_sample, WriteSample write_sample, size_t &generated) {
    if (this->reset_pending_.exchange(false, std::memory_order_relaxed) && !this->reset()) {
      this->reset_pending_.store(true, std::memory_order_relaxed);
      return false;
    }
    generated = 0;
    size_t input_position = 0;
    while (input_position < input_samples) {
      const size_t available = std::min(input_samples - input_position, INPUT_BLOCK_SAMPLES - this->pending_count_);
      const size_t old_pending = this->pending_count_;
      const size_t complete_samples = ((old_pending + available) / 3) * 3;
      if (complete_samples == 0) {
        for (size_t i = 0; i < available; ++i)
          this->pending_[this->pending_count_++] = static_cast<float>(read_sample(input_position + i));
        input_position += available;
        continue;
      }
      for (size_t i = 0; i < old_pending; ++i)
        this->input_[i] = this->pending_[i];
      const size_t consumed = complete_samples - old_pending;
      for (size_t i = 0; i < consumed; ++i)
        this->input_[old_pending + i] = static_cast<float>(read_sample(input_position + i));
      input_position += consumed;
      this->pending_count_ = available - consumed;
      for (size_t i = 0; i < this->pending_count_; ++i)
        this->pending_[i] = static_cast<float>(read_sample(input_position + i));
      input_position += this->pending_count_;

      const size_t output_samples = complete_samples / 3;
      dsps_fird_f32_ae32(&this->fir_, this->input_.data(), this->output_.data(), output_samples);
      for (size_t i = 0; i < output_samples; ++i) {
        const int32_t sample = std::clamp<int32_t>(std::lround(this->output_[i]), -32768, 32767);
        write_sample(generated + i, static_cast<int16_t>(sample));
      }
      generated += output_samples;
    }
    return true;
  }

  // 63-tap Hamming-windowed FIR, 6.5 kHz cutoff, Q14 coefficients.
  static constexpr std::array<int16_t, 63> COEFFICIENTS = {
      13,   5,    -7,   -18,  -19,  -4,   22,   41,  33,   -8,   -61,  -85,  -46,  47,   137,  146,
      38,   -138, -262, -215, 23,   324,  467,  279, -213, -737, -881, -325, 945,  2560, 3911, 4440,
      3911, 2560, 945,  -325, -881, -737, -213, 279, 467,  324,  23,   -215, -262, -138, 38,   146,
      137,  47,   -46,  -85,  -61,  -8,   33,   41,  22,   -4,   -19,  -18,  -7,   5,    13,
  };

  static int16_t decode(const uint8_t *input, size_t i) {
    return static_cast<int16_t>(static_cast<uint16_t>(input[2 * i]) | (static_cast<uint16_t>(input[2 * i + 1]) << 8));
  }

  fir_f32_t fir_{};
  alignas(16) std::array<float, 64> coefficients_{};
  alignas(16) std::array<float, 64> delay_{};
  static constexpr size_t INPUT_BLOCK_SAMPLES = 480;
  alignas(16) std::array<float, INPUT_BLOCK_SAMPLES> input_{};
  alignas(16) std::array<float, INPUT_BLOCK_SAMPLES / 3> output_{};
  float pending_[2]{};
  size_t pending_count_{0};
  std::atomic<bool> reset_pending_{false};
};
#endif

}  // namespace esphome::audio
