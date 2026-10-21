#pragma once

#include "esphome/core/defines.h"

#ifdef USE_TFLITE_MICRO_HELPER

#include <cstddef>
#include <string>

#include "tensorflow/lite/c/common.h"

namespace esphome::tflite_output_processor {

/// @brief Result of model output processing.
struct ProcessedOutput {
  float value;       // e.g., class index or raw value
  float confidence;  // e.g., probability or max value
};

/// @brief Process a raw float32 output buffer according to output_processing.
/// num_classes is the number of elements in output_data (the model output size).
/// scale_factor divides the reported class value when the model output is scaled.
ProcessedOutput process_output(const float *output_data, int num_classes, const std::string &output_processing,
                               float scale_factor, bool debug);

/// @brief Process a model output tensor: dequantizes uint8/int8 tensors, then delegates to the
/// float overload. Returns {0.0f, 0.0f} on error.
ProcessedOutput process_output(TfLiteTensor *output_tensor, const std::string &output_processing, float scale_factor,
                               bool debug);

}  // namespace esphome::tflite_output_processor

#endif  // USE_TFLITE_MICRO_HELPER
