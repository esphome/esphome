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
  // The same confidence rule as the image kind: the model's own probability when its output is one,
  // the softmax of the winner when it holds logits.
  const tflite_output_processor::ProcessedOutput result = tflite_output_processor::process_output(
      tensor, tflite_output_processor::OutputProcessing::OUTPUT_PROCESSING_AUTO_DETECT, 1.0f, false);
  if (!result.valid)
    return false;
  this->class_id_ = static_cast<int>(result.value);
  this->confidence_ = result.confidence;
  this->value_ = result.value;
  return true;
}

}  // namespace esphome::tflite_tabular

#endif  // USE_TFLITE_MICRO_HELPER
