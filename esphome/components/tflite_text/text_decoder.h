#pragma once

#include "esphome/core/defines.h"

#ifdef USE_TFLITE_MICRO_HELPER

#include <cstddef>

#include "esphome/components/tflite_output_processor/output_processor.h"

namespace esphome::tflite_text {

/// @brief A two-entry output holds start/end positions; a wider one is classified.
class TextDecoder final : public tflite_output_processor::OutputDecoder {
 public:
  bool decode(TfLiteTensor *const *outputs, size_t count) override;

  bool is_position() const { return this->position_; }
  int start() const { return this->start_; }
  int end() const { return this->end_; }
  int class_id() const { return this->class_id_; }
  float confidence() const { return this->confidence_; }

 protected:
  bool position_{false};
  int start_{0};
  int end_{0};
  int class_id_{0};
  float confidence_{0.0f};
};

}  // namespace esphome::tflite_text

#endif  // USE_TFLITE_MICRO_HELPER
