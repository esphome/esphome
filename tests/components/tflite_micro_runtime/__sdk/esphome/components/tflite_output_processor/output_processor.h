#pragma once

#include <cstddef>
#include "tensorflow/lite/c/common.h"

namespace esphome::tflite_output_processor {

struct ProcessedOutput {
  float value;
  float confidence;
  bool valid;
};

class OutputDecoder {
 public:
  virtual ~OutputDecoder() = default;
  virtual bool decode(TfLiteTensor *const *outputs, size_t count) = 0;
};

}  // namespace esphome::tflite_output_processor
