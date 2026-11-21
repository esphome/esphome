#include "input_detection.h"

#ifdef USE_TFLITE_MICRO_HELPER

#include <algorithm>
#include <cmath>

#include "esphome/core/log.h"

namespace esphome::tflite_detection {

static const char *const TAG = "tflite_detection";

namespace {
/// One entry of any output tensor the runtime can hand over.
bool tensor_value(const TfLiteTensor *tensor, size_t index, float *out) {
  if (tensor == nullptr || tensor->data.raw == nullptr)
    return false;
  switch (tensor->type) {
    case kTfLiteFloat32:
      *out = tensor->data.f[index];
      return true;
    case kTfLiteUInt8:
      *out = (static_cast<int>(tensor->data.uint8[index]) - tensor->params.zero_point) * tensor->params.scale;
      return true;
    case kTfLiteInt8:
      *out = (static_cast<int>(tensor->data.int8[index]) - tensor->params.zero_point) * tensor->params.scale;
      return true;
    case kTfLiteInt32:
      *out = static_cast<float>(tensor->data.i32[index]);
      return true;
    default:
      return false;
  }
}

float intersection_over_union(const Detection &a, const Detection &b) {
  const float y_min = std::max(a.y_min, b.y_min);
  const float x_min = std::max(a.x_min, b.x_min);
  const float y_max = std::min(a.y_max, b.y_max);
  const float x_max = std::min(a.x_max, b.x_max);
  const float intersection = std::max(0.0f, y_max - y_min) * std::max(0.0f, x_max - x_min);
  const float area_a = std::max(0.0f, a.y_max - a.y_min) * std::max(0.0f, a.x_max - a.x_min);
  const float area_b = std::max(0.0f, b.y_max - b.y_min) * std::max(0.0f, b.x_max - b.x_min);
  const float union_area = area_a + area_b - intersection;
  return union_area > 0.0f ? intersection / union_area : 0.0f;
}
}  // namespace

void DetectionDecoder::insert_(const Detection &detection) {
  for (size_t i = 0; i < this->detection_count_; i++) {
    if (this->detections_[i].class_id == detection.class_id &&
        intersection_over_union(this->detections_[i], detection) > this->iou_threshold_)
      return;
  }
  size_t position = this->detection_count_;
  while (position > 0 && this->detections_[position - 1].score < detection.score)
    position--;
  if (position >= this->max_detections_)
    return;
  const size_t last = std::min(this->detection_count_, this->max_detections_ - 1);
  for (size_t i = last; i > position; i--)
    this->detections_[i] = this->detections_[i - 1];
  this->detections_[position] = detection;
  if (this->detection_count_ < this->max_detections_)
    this->detection_count_++;
}

bool DetectionDecoder::decode(TfLiteTensor *const *outputs, size_t count) {
  this->detection_count_ = 0;
  if (count != 4) {
    ESP_LOGE(TAG, "A detection model needs 4 outputs, this one has %zu", count);
    return false;
  }
  const TfLiteTensor *boxes = outputs[0];
  if (boxes->dims == nullptr || boxes->dims->size != 3 || boxes->dims->data[2] != 4) {
    ESP_LOGE(TAG, "The boxes tensor is not [1, N, 4]");
    return false;
  }
  float total = 0.0f;
  if (!tensor_value(outputs[3], 0, &total)) {
    ESP_LOGE(TAG, "The detection count tensor could not be read");
    return false;
  }
  const int available = std::min(boxes->dims->data[1], static_cast<int>(total));
  for (int index = 0; index < available; index++) {
    float score = 0.0f;
    float class_id = 0.0f;
    Detection detection;
    if (!tensor_value(outputs[2], index, &score) || !tensor_value(outputs[1], index, &class_id) ||
        !tensor_value(boxes, static_cast<size_t>(index) * 4 + 0, &detection.y_min) ||
        !tensor_value(boxes, static_cast<size_t>(index) * 4 + 1, &detection.x_min) ||
        !tensor_value(boxes, static_cast<size_t>(index) * 4 + 2, &detection.y_max) ||
        !tensor_value(boxes, static_cast<size_t>(index) * 4 + 3, &detection.x_max)) {
      ESP_LOGE(TAG, "Detection %d could not be read from the output tensors", index);
      return false;
    }
    if (score < this->score_threshold_)
      continue;
    detection.class_id = static_cast<int>(class_id);
    detection.score = score;
    this->insert_(detection);
  }
  return true;
}

}  // namespace esphome::tflite_detection

#endif  // USE_TFLITE_MICRO_HELPER
