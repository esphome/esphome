#pragma once

#include "esphome/core/defines.h"

#ifdef USE_TFLITE_MICRO_HELPER

#include "tensorflow/lite/c/common.h"

namespace esphome::tflite_micro_helper::debug_utils {

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

}  // namespace esphome::tflite_micro_helper::debug_utils

#endif  // USE_TFLITE_MICRO_HELPER
