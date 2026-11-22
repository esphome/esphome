#include "input_text.h"

#ifdef USE_TFLITE_MICRO_HELPER

#include <algorithm>
#include <cstring>

#include "esphome/core/log.h"

namespace esphome::tflite_text {

static const char *const TAG = "tflite_text";

bool TextInputPipeline::fill_all(TfLiteTensor *const *inputs, size_t count, const uint8_t *src, size_t size) {
  if (count < 1 || count > 3) {
    ESP_LOGE(TAG, "A text model has 1 to 3 inputs, this one has %zu", count);
    return false;
  }
  for (size_t index = 0; index < count; index++) {
    if (inputs[index]->type != kTfLiteInt32 || inputs[index]->dims == nullptr || inputs[index]->dims->size != 2 ||
        inputs[index]->dims->data[0] != 1) {
      ESP_LOGE(TAG, "Input %zu is not a [1, length] int32 tensor", index);
      return false;
    }
  }
  if (size > static_cast<size_t>(inputs[0]->bytes)) {
    ESP_LOGE(TAG, "%zu bytes of tokens do not fit the %d-byte input", size, inputs[0]->bytes);
    return false;
  }
  // Clear first, so a shorter request cannot reuse the previous call's tail.
  memset(inputs[0]->data.i32, 0, inputs[0]->bytes);
  memcpy(inputs[0]->data.i32, src, size);
  if (count > 1)
    std::fill_n(inputs[1]->data.i32, inputs[1]->bytes / 4, 1);
  if (count > 2)
    std::fill_n(inputs[2]->data.i32, inputs[2]->bytes / 4, 0);
  return true;
}

}  // namespace esphome::tflite_text

#endif  // USE_TFLITE_MICRO_HELPER
