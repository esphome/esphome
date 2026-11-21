#include "input_tabular.h"

#ifdef USE_TFLITE_MICRO_HELPER

#include <cstring>

#include "esphome/core/log.h"

namespace esphome::tflite_tabular {

static const char *const TAG = "tflite_tabular";

bool TabularInputPipeline::fill(TfLiteTensor *input, const uint8_t *src, size_t size) {
  if (input == nullptr || src == nullptr) {
    ESP_LOGE(TAG, "fill called without a tensor or features");
    return false;
  }
  if (input->type != kTfLiteFloat32 || input->data.f == nullptr) {
    ESP_LOGE(TAG, "The input tensor is not float32");
    return false;
  }
  if (size != input->bytes) {
    ESP_LOGE(TAG, "The feature vector is %zu bytes, the tensor wants %zu", size, input->bytes);
    return false;
  }
  memcpy(input->data.f, src, size);
  return true;
}

}  // namespace esphome::tflite_tabular

#endif  // USE_TFLITE_MICRO_HELPER
