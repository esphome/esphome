#pragma once

#include "esphome/core/defines.h"

#ifdef USE_TFLITE_MICRO_HELPER

#include <cstddef>
#include <cstdint>

#include "esphome/components/tflite_micro_runtime/tflite_micro_runtime.h"

namespace esphome::tflite_audio {

/// Audio (streaming) parameters, read by the consumer that owns the interpreter.
struct AudioModelConfig {
  uint8_t probability_cutoff{128};  // quantized 0-255 (from 0.0-1.0)
  size_t sliding_window_size{10};
  uint8_t features_step_size{20};  // ms between feature slices
  size_t feature_count{40};        // Mel filterbank bins
};

/// @brief The runtime helper for a streaming audio model.
///
/// The streaming consumer owns the interpreter and reads the tuning values from
/// here; they are not part of the shared runtime, so an image, text or raw model
/// does not carry them.
class TFLiteAudio final : public tflite_micro_runtime::TFLiteMicroHelper {
 public:
  void set_probability_cutoff(float f);  // 0.0-1.0, quantized internally
  void set_sliding_window_size(size_t n);
  void set_features_step_size(uint8_t ms);
  void set_feature_count(size_t n);
  const AudioModelConfig &get_audio_config() const { return this->audio_config_; }

 protected:
  AudioModelConfig audio_config_;
};

}  // namespace esphome::tflite_audio

#endif  // USE_TFLITE_MICRO_HELPER
