#pragma once

#include <cstdint>
#include <cstddef>
#include "tensorflow/lite/c/common.h"
#include "esphome/core/log.h"

namespace esphome::tflite_micro_helper {

/**
 * @brief Debug utilities for TFLite Micro Helper
 *
 * Provides debug logging and profiling functions.
 * Function bodies are in debug_utils.cpp to avoid logging macros in headers.
 */
namespace debug_utils {

void log_arena_usage(const char *tag, size_t used, size_t total);
void log_model_info(const char *tag, const char *model_name, size_t model_size, size_t tensor_arena_size);
void log_inference_time(const char *tag, uint32_t time_ms);

}  // namespace debug_utils

/// Helper function to convert TfLiteType to string (namespace-agnostic for compatibility).
inline const char *tflite_type_to_string(TfLiteType type) {
  switch (type) {
    case kTfLiteFloat32:
      return "kTfLiteFloat32";
    case kTfLiteUInt8:
      return "kTfLiteUInt8";
    case kTfLiteInt8:
      return "kTfLiteInt8";
    case kTfLiteInt32:
      return "kTfLiteInt32";
    case kTfLiteInt64:
      return "kTfLiteInt64";
    case kTfLiteBool:
      return "kTfLiteBool";
    case kTfLiteString:
      return "kTfLiteString";
    case kTfLiteNoType:
      return "kTfLiteNoType";
    default:
      return "Unknown";
  }
}

}  // namespace esphome::tflite_micro_helper
