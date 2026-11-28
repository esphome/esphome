#pragma once

#include "esphome/core/defines.h"

#include <cmath>
#include <cstddef>
#include <cstdint>

#ifdef USE_TFLITE_MICRO_HELPER
#include "esphome/components/tflite_micro_runtime/tflite_micro_runtime.h"
#include "esphome/components/tflite_output_processor/output_processor.h"
#endif

namespace esphome::tflite_waveform {

/// Full scale of a 16-bit sample, which a model sees as 1.0.
static constexpr float PCM_FULL_SCALE = 32768.0f;

/// A 16-bit sample as the model's input, in [-1.0, 1.0).
inline float pcm_to_float(int16_t sample) { return static_cast<float>(sample) / PCM_FULL_SCALE; }

/// A model output as a 16-bit sample, saturated; NaN becomes silence.
inline int16_t float_to_pcm(float value) {
  const float scaled = value * PCM_FULL_SCALE;
  if (std::isnan(scaled))
    return 0;
  if (scaled >= 32767.0f)
    return 32767;
  if (scaled <= -32768.0f)
    return -32768;
  return static_cast<int16_t>(std::lroundf(scaled));
}

/// Quantize a value to [low, high] with the tensor's scale (which must not be 0) and zero point.
inline int quantize(float value, float scale, int zero_point, int low, int high) {
  // Rounded before the zero point is added, as TensorFlow Lite quantizes.
  const float scaled = value / scale;
  if (scaled >= static_cast<float>(high - zero_point))
    return high;
  if (scaled <= static_cast<float>(low - zero_point))
    return low;
  return static_cast<int>(std::lroundf(scaled)) + zero_point;
}

#ifdef USE_TFLITE_MICRO_HELPER
/// @brief Fills the model's input from one frame of 16-bit PCM samples.
class WaveformInputPipeline final : public tflite_micro_runtime::InputPipeline {
 public:
  /// @param size  Bytes in @p src, two per sample of the input tensor.
  bool fill(TfLiteTensor *input, const uint8_t *src, size_t size) override;
  const char *name() const override { return "waveform"; }
};

/// @brief Turns the model's single output into one frame of 16-bit PCM samples.
class WaveformDecoder final : public tflite_output_processor::OutputDecoder {
 public:
  /// @param samples  Storage for @p capacity samples, from code generation.
  WaveformDecoder(int16_t *samples, size_t capacity) : samples_(samples), capacity_(capacity) {}

  bool decode(TfLiteTensor *const *outputs, size_t count) override;

  const int16_t *samples() const { return this->samples_; }
  size_t sample_count() const { return this->count_; }

 protected:
  int16_t *samples_;
  size_t capacity_;
  size_t count_{0};
};
#endif

}  // namespace esphome::tflite_waveform
