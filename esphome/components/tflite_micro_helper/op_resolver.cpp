#include "op_resolver.h"

namespace esphome {
namespace tflite_micro_helper {

void log_op_registering(const char *tag, const char *op_name) { ESP_LOGD(tag, "Registering op: %s", op_name); }

void log_op_unavailable(const char *tag, const char *op_name) {
  ESP_LOGW(tag, "Operator %s is not available in TFLite Micro", op_name);
}

void log_op_unknown(const char *tag, const char *op_name) {
  ESP_LOGE(tag, "Unknown or unsupported operator: %s", op_name);
}

void log_op_failed(const char *tag, const char *op_name) { ESP_LOGE(tag, "Failed to add operator: %s", op_name); }

}  // namespace tflite_micro_helper
}  // namespace esphome