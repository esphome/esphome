#pragma once

#include "tensorflow/lite/c/common.h"

namespace esphome::tflite_runtime_testing {
/// What the engine double does next, and how often it ran.
struct Engine {
  TfLiteType output_type{kTfLiteFloat32};
  TfLiteStatus invoke_status{kTfLiteOk};
  bool load_succeeds{true};
  bool allocation_succeeds{true};
  unsigned invocations{0};
  unsigned unloads{0};
};
inline Engine engine;
}  // namespace esphome::tflite_runtime_testing
