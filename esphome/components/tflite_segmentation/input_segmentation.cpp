#include "input_segmentation.h"

#ifdef USE_TFLITE_MICRO_HELPER

#include <cmath>

#include "esphome/core/log.h"

namespace esphome::tflite_segmentation {

static const char *const TAG = "tflite_segmentation";

bool SegmentationDecoder::decode(TfLiteTensor *const *outputs, size_t count) {
  if (count != 1) {
    ESP_LOGE(TAG, "A segmentation model needs 1 output, this one has %zu", count);
    return false;
  }
  TfLiteTensor *mask = outputs[0];
  if (mask->dims == nullptr || mask->dims->size != 4) {
    ESP_LOGE(TAG, "The mask tensor is not [1, height, width, classes]");
    return false;
  }
  const size_t height = mask->dims->data[1];
  const size_t width = mask->dims->data[2];
  const size_t classes = mask->dims->data[3];
  if (height == 0 || width == 0 || classes == 0 || classes > 255) {
    ESP_LOGE(TAG, "Unusable mask %zux%zu with %zu classes", width, height, classes);
    return false;
  }
  const size_t pixels = width * height;
  // Allocated once, when the shape is first seen; a decoder is reused per model.
  if (this->width_ != width || this->height_ != height || this->classes_ != classes) {
    this->labels_ = std::make_unique<uint8_t[]>(pixels);
    this->width_ = width;
    this->height_ = height;
    this->classes_ = classes;
  }
  for (size_t pixel = 0; pixel < pixels; pixel++) {
    float best = -INFINITY;
    size_t best_class = 0;
    for (size_t class_index = 0; class_index < classes; class_index++) {
      float value;
      if (!tflite_output_processor::read_tensor_value(mask, pixel * classes + class_index, &value)) {
        ESP_LOGE(TAG, "The mask tensor could not be read");
        return false;
      }
      if (value > best) {
        best = value;
        best_class = class_index;
      }
    }
    this->labels_[pixel] = static_cast<uint8_t>(best_class);
  }
  return true;
}

}  // namespace esphome::tflite_segmentation

#endif  // USE_TFLITE_MICRO_HELPER
