#include "op_resolver_utils.h"
#include "esphome/core/log.h"

namespace esphome {
namespace tflite_micro_helper {

static const char *const TAG = "op_resolver";

void op_resolver_log_register(const char *tag, const char *op_name) { ESP_LOGD(tag, "Registering op: %s", op_name); }

void op_resolver_log_unavailable(const char *tag, const char *op_name) {
  ESP_LOGW(tag, "Operator %s is not available in TFLite Micro", op_name);
}

void op_resolver_log_unknown(const char *tag, const char *op_name) {
  ESP_LOGE(tag, "Unknown or unsupported operator: %s", op_name);
}

void op_resolver_log_failed(const char *tag, const char *op_name) {
  ESP_LOGE(tag, "Failed to add operator: %s", op_name);
}

}  // namespace tflite_micro_helper
}  // namespace esphome
