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

bool suppresses(const Detection &kept, const Detection &candidate, float iou_threshold) {
  return kept.class_id == candidate.class_id && intersection_over_union(kept, candidate) > iou_threshold;
}

#ifdef USE_TFLITE_MICRO_HELPER

namespace {
ESPHOME_LOG_TAG(TAG, "tflite_detection");

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

bool DetectionDecoder::decode(TfLiteTensor *const *outputs, size_t count) {
  this->detection_count_ = 0;
  if (this->format_ != DetectionFormat::DETECTION_FORMAT_SSD) {
    if (count != 1) {
      ESP_LOGE(TAG, "A YOLO model needs 1 output, this one has %zu", count);
      return false;
    }
    return this->decode_yolo_(outputs[0]);
  }
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
  bool readable = true;
  auto get = [&](size_t index, Detection &detection) {
    float class_id = 0.0f;
    if (!tensor_value(outputs[2], index, &detection.score) || !tensor_value(outputs[1], index, &class_id) ||
        !tensor_value(boxes, index * 4 + 0, &detection.y_min) ||
        !tensor_value(boxes, index * 4 + 1, &detection.x_min) ||
        !tensor_value(boxes, index * 4 + 2, &detection.y_max) ||
        !tensor_value(boxes, index * 4 + 3, &detection.x_max)) {
      readable = false;
      return false;
    }
    detection.class_id = static_cast<int>(class_id);
    // Written this way round so a NaN score is skipped.
    return detection.score >= this->score_threshold_;
  };
  const size_t selected =
      select_detections(this->detections_.get(), this->max_detections_, this->iou_threshold_, available, get);
  if (!readable) {
    ESP_LOGE(TAG, "A detection could not be read from the output tensors");
    return false;
  }
  this->detection_count_ = selected;
  return true;
}

bool DetectionDecoder::decode_yolo_(const TfLiteTensor *tensor) {
  const bool v5 = this->format_ == DetectionFormat::DETECTION_FORMAT_YOLOV5;
  const size_t fields_before_classes = v5 ? 5 : 4;
  if (tensor->dims == nullptr || tensor->dims->size != 3 || tensor->dims->data[0] != 1 || tensor->dims->data[1] <= 0 ||
      tensor->dims->data[2] <= 0) {
    ESP_LOGE(TAG, "The candidate tensor does not have 3 dimensions");
    return false;
  }
  // yolov5 stores one candidate per row, yolov8 one per column.
  const size_t fields = tensor->dims->data[v5 ? 2 : 1];
  const size_t candidates = tensor->dims->data[v5 ? 1 : 2];
  float probe = 0.0f;
  if (fields <= fields_before_classes || !tensor_value(tensor, 0, &probe)) {
    ESP_LOGE(TAG, "The candidate tensor has no class scores or an unsupported type");
    return false;
  }
  auto get = [&](size_t index, Detection &detection) {
    auto value = [&](size_t field) {
      // The shape and type were checked above, so the read cannot fail.
      float out = 0.0f;
      tensor_value(tensor, v5 ? index * fields + field : field * candidates + index, &out);
      return out;
    };
    return yolo_candidate(value, fields - fields_before_classes, v5, this->score_threshold_, detection);
  };
  this->detection_count_ =
      select_detections(this->detections_.get(), this->max_detections_, this->iou_threshold_, candidates, get);
  return true;
}

#endif  // USE_TFLITE_MICRO_HELPER

}  // namespace esphome::tflite_detection
