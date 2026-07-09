#pragma once

#include "esphome/core/hal.h"  // For millis()
#include "esphome/core/log.h"
#include "tensorflow/lite/c/common.h"  // For TfLiteType

namespace esphome {
namespace tflite_micro_helper {

// RAII ScopedDuration -- replaces DURATION_START/END/LOG macros
class ScopedDuration {
 public:
  explicit ScopedDuration(const char *tag) : tag_(tag), start_(esphome::millis()) {}

  uint32_t elapsed() const { return esphome::millis() - this->start_; }

  void log_duration(const char *func) { ESP_LOGD(this->tag_, "%s duration: %lums", func, this->elapsed()); }

  void log(const char *msg, uint32_t val) { ESP_LOGD(this->tag_, "%s: %lums", msg, val); }

 private:
  const char *tag_;
  uint32_t start_;
};

}  // namespace tflite_micro_helper
}  // namespace esphome

// Helper function to convert TfLiteType to string (namespace-agnostic for compatibility)
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
