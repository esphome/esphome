#pragma once

#include "esphome/core/defines.h"

#ifdef USE_TFLITE_MICRO_HELPER

#include <cstddef>
#include <cstdint>

#include "esphome/components/tflite_output_processor/output_processor.h"
#include "esphome/core/helpers.h"

namespace esphome::tflite_segmentation {

/// @brief Turns a [1, height, width, classes] mask into one class id per pixel.
class SegmentationDecoder final : public tflite_output_processor::OutputDecoder {
 public:
  bool decode(TfLiteTensor *const *outputs, size_t count) override;

  const uint8_t *labels() const { return this->labels_.get(); }
  size_t width() const { return this->width_; }
  size_t height() const { return this->height_; }
  size_t class_count() const { return this->classes_; }

 protected:
  RAMUniquePtr<uint8_t[]> labels_;
  size_t width_{0};
  size_t height_{0};
  size_t classes_{0};
};

}  // namespace esphome::tflite_segmentation

#endif  // USE_TFLITE_MICRO_HELPER
