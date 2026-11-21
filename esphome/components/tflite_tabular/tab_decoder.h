#pragma once

#include "esphome/core/defines.h"

#ifdef USE_TFLITE_MICRO_HELPER

#include <cstddef>

#include "esphome/components/tflite_output_processor/output_processor.h"

namespace esphome::tflite_tabular {

/// @brief A [1, 1] output is a regression value; a wider one is classified.
class TabularDecoder final : public tflite_output_processor::OutputDecoder {
 public:
  bool decode(TfLiteTensor *const *outputs, size_t count) override;

  bool is_regression() const { return this->regression_; }
  float value() const { return this->value_; }
  float confidence() const { return this->confidence_; }
  int class_id() const { return this->class_id_; }

 protected:
  bool regression_{false};
  float value_{0.0f};
  float confidence_{0.0f};
  int class_id_{0};
};

}  // namespace esphome::tflite_tabular

#endif  // USE_TFLITE_MICRO_HELPER
