#pragma once

#include "esphome/core/defines.h"

#ifdef USE_TFLITE_MICRO_HELPER

#include <cstddef>
#include <cstdint>

#include "esphome/components/tflite_micro_runtime/tflite_micro_runtime.h"

namespace esphome::tflite_text {

/// @brief Fills a text model's int32 inputs from token ids.
///
/// Input 0 receives the tokens, input 1 an all-ones attention mask and input 2 an
/// all-zero segment mask, the layout the exported BERT-style models expect.
class TextInputPipeline final : public tflite_micro_runtime::InputPipeline {
 public:
  bool fill(TfLiteTensor *input, const uint8_t *src, size_t size) override { return false; }
  bool fill_all(TfLiteTensor *const *inputs, size_t count, const uint8_t *src, size_t size) override;
  const char *name() const override { return "text"; }
};

}  // namespace esphome::tflite_text

#endif  // USE_TFLITE_MICRO_HELPER
