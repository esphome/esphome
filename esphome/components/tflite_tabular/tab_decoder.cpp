#include "tab_decoder.h"

#ifdef USE_TFLITE_MICRO_HELPER

#include <cmath>

#include "esphome/core/log.h"

namespace esphome::tflite_tabular {

static const char *const TAG = "tflite_tabular";

bool TabularDecoder::decode(TfLiteTensor *const *outputs, size_t count) {
  if (count != 1) {
    ESP_LOGE(TAG, "A tabular model needs 1 output, this one has %zu", count);
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
  if (classes == 1) {
    this->regression_ = true;
    return tflite_output_processor::read_tensor_value(tensor, 0, &this->value_);
  }
  this->regression_ = false;
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
  this->confidence_ = best;
  this->value_ = static_cast<float>(this->class_id_);
  return true;
}

}  // namespace esphome::tflite_tabular

#endif  // USE_TFLITE_MICRO_HELPER
