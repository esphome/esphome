#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#if defined(USE_DSP_DECIMATOR) && defined(USE_ESP_IDF)
#include <cmath>
#include <dsps_fir.h>
#endif

namespace esphome::resampler {

#if defined(USE_DSP_DECIMATOR) && defined(USE_ESP_IDF)
// ESP-DSP's Xtensa FPU FIR is faster than the scalar Q14 loop. Its floating
// result is saturated here; the library's s16 FIR wraps on full-scale input.
class DspDecimator3 {
 public:
  bool reset() {
    this->coefficients_[0] = 0.0f;  // 64th tap required by the DSP FIR
    for (size_t i = 0; i < 63; ++i)
      this->coefficients_[i + 1] = static_cast<float>(COEFFICIENTS[62 - i]) / 16384.0f;
    this->pending_count_ = 0;
    return dsps_fird_init_f32(&this->fir_, this->coefficients_.data(), this->delay_.data(), 64, 3) == ESP_OK;
  }

  void process(const std::vector<uint8_t> &input, std::vector<uint8_t> &output) {
    const size_t input_samples = input.size() / sizeof(int16_t);
    const size_t old_pending = this->pending_count_;
    const size_t complete_samples = ((old_pending + input_samples) / 3) * 3;
    const size_t output_samples = complete_samples / 3;
    if (output_samples == 0) {
      for (size_t i = 0; i < input_samples; ++i)
        this->pending_[this->pending_count_++] = static_cast<float>(decode(input, i));
      output.clear();
      return;
    }
    this->input_.resize(complete_samples);
    for (size_t i = 0; i < old_pending; ++i)
      this->input_[i] = this->pending_[i];
    const size_t consumed = complete_samples - old_pending;
    for (size_t i = 0; i < consumed; ++i)
      this->input_[old_pending + i] = static_cast<float>(decode(input, i));
    this->pending_count_ = input_samples - consumed;
    for (size_t i = 0; i < this->pending_count_; ++i)
      this->pending_[i] = static_cast<float>(decode(input, consumed + i));

    output.resize(output_samples * sizeof(int16_t));
    this->output_.resize(output_samples);
    dsps_fird_f32_ae32(&this->fir_, this->input_.data(), this->output_.data(), output_samples);
    for (size_t i = 0; i < output_samples; ++i) {
      const int32_t sample = std::clamp<int32_t>(std::lround(this->output_[i]), -32768, 32767);
      output[2 * i] = static_cast<uint8_t>(sample);
      output[2 * i + 1] = static_cast<uint8_t>(sample >> 8);
    }
  }

 private:
  // 63-tap Hamming-windowed FIR, 6.5 kHz cutoff, Q14 coefficients.
  static constexpr std::array<int16_t, 63> COEFFICIENTS = {
      13,   5,    -7,   -18,  -19,  -4,   22,   41,  33,   -8,   -61,  -85,  -46,  47,   137,  146,
      38,   -138, -262, -215, 23,   324,  467,  279, -213, -737, -881, -325, 945,  2560, 3911, 4440,
      3911, 2560, 945,  -325, -881, -737, -213, 279, 467,  324,  23,   -215, -262, -138, 38,   146,
      137,  47,   -46,  -85,  -61,  -8,   33,   41,  22,   -4,   -19,  -18,  -7,   5,    13,
  };

  static int16_t decode(const std::vector<uint8_t> &input, size_t i) {
    return static_cast<int16_t>(static_cast<uint16_t>(input[2 * i]) | (static_cast<uint16_t>(input[2 * i + 1]) << 8));
  }

  fir_f32_t fir_{};
  alignas(16) std::array<float, 64> coefficients_{};
  alignas(16) std::array<float, 64> delay_{};
  std::vector<float> input_;
  std::vector<float> output_;
  float pending_[2]{};
  size_t pending_count_{0};
};
#endif

}  // namespace esphome::resampler
