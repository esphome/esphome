#include "op_resolver.h"

#ifdef USE_TFLITE_MICRO_HELPER

#include "esphome/core/log.h"

namespace esphome::tflite_op_processor {

void log_op_registering(const char *tag, const char *op_name) { ESP_LOGD(tag, "Registering op: %s", op_name); }

void log_op_unavailable(const char *tag, const char *op_name) {
  ESP_LOGW(tag, "Operator %s is not available in TFLite Micro", op_name);
}

void log_op_not_compiled(const char *tag, const char *op_name) {
  ESP_LOGW(tag, "Operator %s is not compiled into this firmware (no configured model uses it)", op_name);
}

void log_op_unknown(const char *tag, const char *op_name) {
  ESP_LOGE(tag, "Unknown or unsupported operator: %s", op_name);
}

void log_op_failed(const char *tag, const char *op_name) { ESP_LOGE(tag, "Failed to add operator: %s", op_name); }

}  // namespace esphome::tflite_op_processor

#endif  // USE_TFLITE_MICRO_HELPER
