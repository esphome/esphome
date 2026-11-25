#include "input_detection.h"

#include <algorithm>

#ifdef USE_TFLITE_MICRO_HELPER
#include "esphome/core/log.h"
#endif

namespace esphome::tflite_detection {

namespace {
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

size_t insert_detection(Detection *detections, size_t count, size_t max_detections, float iou_threshold,
                        const Detection &candidate) {
  if (max_detections == 0)
    return 0;
  // A higher-scored box of the same class already covers this candidate.
  for (size_t i = 0; i < count; i++) {
    if (detections[i].class_id == candidate.class_id && detections[i].score >= candidate.score &&
        intersection_over_union(detections[i], candidate) > iou_threshold)
      return count;
  }
  // This candidate suppresses the lower-scored overlapping boxes instead.
  size_t kept = 0;
  for (size_t i = 0; i < count; i++) {
    if (detections[i].class_id == candidate.class_id && detections[i].score < candidate.score &&
        intersection_over_union(detections[i], candidate) > iou_threshold)
      continue;
    detections[kept++] = detections[i];
  }
  size_t position = kept;
  while (position > 0 && detections[position - 1].score < candidate.score)
    position--;
  if (position >= max_detections)
    return kept;
  const size_t last = std::min(kept, max_detections - 1);
  for (size_t i = last; i > position; i--)
    detections[i] = detections[i - 1];
  detections[position] = candidate;
  return std::min(kept + 1, max_detections);
}

#ifdef USE_TFLITE_MICRO_HELPER

namespace {
static const char *const TAG = "tflite_detection";

/// Number of entries a tensor can hold; 0 for a tensor without usable dimensions.
size_t tensor_elements(const TfLiteTensor *tensor) {
  if (tensor == nullptr || tensor->dims == nullptr)
    return 0;
  size_t count = 1;
  for (int i = 0; i < tensor->dims->size; i++) {
    if (tensor->dims->data[i] <= 0)
      return 0;
    count *= static_cast<size_t>(tensor->dims->data[i]);
  }
  return count;
}

/// One entry of any output tensor the runtime can hand over.
bool tensor_value(const TfLiteTensor *tensor, size_t index, float *out) {
  if (tensor == nullptr || tensor->data.raw == nullptr || index >= tensor_elements(tensor))
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
}  // namespace

void DetectionDecoder::insert_(const Detection &detection) {
  this->detection_count_ = insert_detection(this->detections_.get(), this->detection_count_, this->max_detections_,
                                            this->iou_threshold_, detection);
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
  const size_t candidates = static_cast<size_t>(boxes->dims->data[1]);
  // The count is a float: a NaN or negative one means no detections, and the cast
  // of an out-of-range float to size_t is undefined.
  size_t available = 0;
  if (total > 0.0f)
    available = total >= static_cast<float>(candidates) ? candidates : static_cast<size_t>(total);
  // Each per-box tensor must hold at least as many entries as the boxes it describes.
  available = std::min({available, tensor_elements(outputs[1]), tensor_elements(outputs[2])});
  for (size_t index = 0; index < available; index++) {
    float score = 0.0f;
    float class_id = 0.0f;
    Detection detection;
    if (!tensor_value(outputs[2], index, &score) || !tensor_value(outputs[1], index, &class_id) ||
        !tensor_value(boxes, index * 4 + 0, &detection.y_min) ||
        !tensor_value(boxes, index * 4 + 1, &detection.x_min) ||
        !tensor_value(boxes, index * 4 + 2, &detection.y_max) ||
        !tensor_value(boxes, index * 4 + 3, &detection.x_max)) {
      ESP_LOGE(TAG, "Detection %zu could not be read from the output tensors", index);
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

#endif  // USE_TFLITE_MICRO_HELPER

}  // namespace esphome::tflite_detection
