#include "debug_utils.h"

namespace esphome {
namespace tflite_micro_helper {
namespace debug_utils {

void log_arena_usage(const char *tag, size_t used, size_t total) {
  ESP_LOGI(tag, "Tensor Arena: %u / %u bytes used (%.1f%%)", used, total,
           total > 0 ? (float) used / total * 100.0f : 0.0f);
}

void log_model_info(const char *tag, const char *model_name, size_t model_size, size_t tensor_arena_size) {
  ESP_LOGI(tag, "Model: %s, size: %u bytes, tensor_arena: %u bytes", model_name, model_size, tensor_arena_size);
}

void log_inference_time(const char *tag, uint32_t time_ms) { ESP_LOGD(tag, "Inference time: %u ms", time_ms); }

}  // namespace debug_utils
}  // namespace tflite_micro_helper
}  // namespace esphome