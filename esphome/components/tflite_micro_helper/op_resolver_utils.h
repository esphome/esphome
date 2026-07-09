#pragma once

#include "esphome/core/defines.h"

#ifdef USE_TFLITE_MICRO_HELPER

namespace esphome {
namespace tflite_micro_helper {

// Helper functions that perform logging from .cpp files, avoiding ESP_LOG macros in headers.
// The template function in op_resolver.h calls these instead of ESP_LOG directly.
void op_resolver_log_register(const char *tag, const char *op_name);
void op_resolver_log_unavailable(const char *tag, const char *op_name);
void op_resolver_log_unknown(const char *tag, const char *op_name);
void op_resolver_log_failed(const char *tag, const char *op_name);

}  // namespace tflite_micro_helper
}  // namespace esphome

#endif  // USE_TFLITE_MICRO_HELPER
