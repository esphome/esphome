#include "tflite_ctc.h"

#ifdef USE_TFLITE_MICRO_HELPER

#include "esphome/core/log.h"

namespace esphome::tflite_ctc {

ESPHOME_LOG_TAG(TAG, "tflite_ctc");

bool CtcDecoder::decode(TfLiteTensor *const *outputs, size_t count) {
  this->id_count_ = 0;
  if (count != 1) {
    ESP_LOGE(TAG, "A CTC model needs 1 output, this one has %zu", count);
    return false;
  }
  const TfLiteTensor *tensor = outputs[0];
  if (tensor->dims == nullptr || tensor->dims->size != 3 || tensor->dims->data[0] != 1 || tensor->dims->data[1] <= 0 ||
      tensor->dims->data[2] <= 1) {
    ESP_LOGE(TAG, "The output tensor is not [1, steps, classes]");
    return false;
  }
  if (tensor->type != kTfLiteFloat32 && tensor->type != kTfLiteInt8 && tensor->type != kTfLiteUInt8) {
    ESP_LOGE(TAG, "Unsupported output tensor type: %d", tensor->type);
    return false;
  }
  const size_t steps = tensor->dims->data[1];
  const size_t classes = tensor->dims->data[2];
  if (this->blank_ >= classes) {
    ESP_LOGE(TAG, "The blank class %zu is outside the %zu classes", this->blank_, classes);
    return false;
  }
  if (this->capacity_ < steps) {
    this->ids_ = RAMAllocator<uint16_t>().make_unique_array_for_overwrite(steps);
    if (!this->ids_) {
      this->capacity_ = 0;
      ESP_LOGE(TAG, "Could not allocate %zu ids", steps);
      return false;
    }
    this->capacity_ = steps;
  }
  this->id_count_ = ctc_greedy_decode(this->ids_.get(), steps, classes, this->blank_, [tensor](size_t index) {
    // The type was checked above, so the read cannot fail.
    float value = 0.0f;
    tflite_output_processor::read_tensor_value(tensor, index, &value);
    return value;
  });
  return true;
}

std::string CtcDecoder::text() const {
  std::string text;
  if (this->labels_ == nullptr)
    return text;
  for (size_t i = 0; i < this->id_count_; i++)
    text += this->labels_[ctc_label_index(this->ids_[i], this->blank_)];
  return text;
}

}  // namespace esphome::tflite_ctc

#endif  // USE_TFLITE_MICRO_HELPER
