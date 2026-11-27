#include "input_text.h"

#ifdef USE_TFLITE_MICRO_HELPER

#include <algorithm>
#include <cstring>

#include "esphome/core/log.h"

namespace esphome::tflite_text {

ESPHOME_LOG_TAG(TAG, "tflite_text");

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
  int32_t *tokens = inputs[0]->data.i32;
  const size_t length = inputs[0]->bytes / sizeof(int32_t);
  size_t used;
  if (this->tokenizer_ != nullptr) {
    used = this->tokenizer_->encode(reinterpret_cast<const char *>(src), size, tokens, length);
    if (used == 0) {
      ESP_LOGE(TAG, "A %zu-token input is too short for the start and end tokens", length);
      return false;
    }
  } else {
    if (size > static_cast<size_t>(inputs[0]->bytes)) {
      ESP_LOGE(TAG, "%zu bytes of tokens do not fit the %zu-byte input", size, static_cast<size_t>(inputs[0]->bytes));
      return false;
    }
    // Clear first, so a shorter request cannot reuse the previous call's tail.
    memset(tokens, 0, inputs[0]->bytes);
    memcpy(tokens, src, size);
    used = size / sizeof(int32_t);
  }
  if (count > 1) {
    // The model must not attend to the padding.
    const size_t mask_length = inputs[1]->bytes / sizeof(int32_t);
    const size_t attended = std::min(used, mask_length);
    std::fill_n(inputs[1]->data.i32, attended, 1);
    std::fill_n(inputs[1]->data.i32 + attended, mask_length - attended, 0);
  }
  if (count > 2)
    std::fill_n(inputs[2]->data.i32, inputs[2]->bytes / 4, 0);
  return true;
}

}  // namespace esphome::tflite_text

#endif  // USE_TFLITE_MICRO_HELPER
