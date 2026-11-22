#pragma once

#include "esphome/core/defines.h"

#ifdef USE_TFLITE_MICRO_HELPER

#include <cstddef>
#include <cstdint>

#include "esphome/components/tflite_micro_runtime/tflite_micro_runtime.h"

namespace esphome::tflite_tabular {

/// @brief Fills a model's float32 input tensor from a feature vector.
class TabularInputPipeline final : public tflite_micro_runtime::InputPipeline {
 public:
  bool fill(TfLiteTensor *input, const uint8_t *src, size_t size) override;
  const char *name() const override { return "tabular"; }
};

}  // namespace esphome::tflite_tabular

#endif  // USE_TFLITE_MICRO_HELPER
