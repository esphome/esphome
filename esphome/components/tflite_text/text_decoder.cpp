#include "text_decoder.h"

#ifdef USE_TFLITE_MICRO_HELPER

#include <cmath>

#include "esphome/core/log.h"

namespace esphome::tflite_text {

static const char *const TAG = "tflite_text";

bool TextDecoder::decode(TfLiteTensor *const *outputs, size_t count) {
  if (count != 1) {
    ESP_LOGE(TAG, "A text model needs 1 output, this one has %zu", count);
    return false;
  }
  TfLiteTensor *tensor = outputs[0];
  if (tensor->dims == nullptr || tensor->dims->size != 2 || tensor->dims->data[0] != 1) {
    ESP_LOGE(TAG, "The output tensor is not [1, C]");
    return false;
  }
  const int classes = tensor->dims->data[1];
  if (classes <= 0)
    return false;
  if (this->position_output_) {
    if (classes != 2) {
      ESP_LOGE(TAG, "A position output needs 2 entries, this one has %d", classes);
      return false;
    }
    float start;
    float end;
    if (!tflite_output_processor::read_tensor_value(tensor, 0, &start) ||
        !tflite_output_processor::read_tensor_value(tensor, 1, &end))
      return false;
    this->position_ = true;
    this->start_ = static_cast<int>(start);
    this->end_ = static_cast<int>(end);
    return true;
  }
  this->position_ = false;
  // The tensor holds logits, so the winning entry is not a probability: softmax it.
  float best = -INFINITY;
  for (int index = 0; index < classes; index++) {
    float value;
    if (!tflite_output_processor::read_tensor_value(tensor, index, &value))
      return false;
    if (value > best) {
      best = value;
      this->class_id_ = index;
    }
  }
  float sum = 0.0f;
  for (int index = 0; index < classes; index++) {
    float value;
    if (!tflite_output_processor::read_tensor_value(tensor, index, &value))
      return false;
    sum += expf(value - best);
  }
  this->confidence_ = sum > 0.0f ? 1.0f / sum : 0.0f;
  return true;
}

}  // namespace esphome::tflite_text

#endif  // USE_TFLITE_MICRO_HELPER
