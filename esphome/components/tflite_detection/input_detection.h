#pragma once

#include "esphome/core/defines.h"

#ifdef USE_TFLITE_MICRO_HELPER

#include <cstddef>
#include <cstdint>
#include <memory>

#include "esphome/components/tflite_output_processor/output_processor.h"

namespace esphome::tflite_detection {

/// @brief One decoded box, in the model's normalized coordinates.
struct Detection {
  float y_min{0.0f};
  float x_min{0.0f};
  float y_max{0.0f};
  float x_max{0.0f};
  int class_id{0};
  float score{0.0f};
};

/// @brief Decodes a detection model's four outputs: boxes, classes, scores and count.
class DetectionDecoder final : public tflite_output_processor::OutputDecoder {
 public:
  DetectionDecoder(float score_threshold, size_t max_detections, float iou_threshold)
      : score_threshold_(score_threshold),
        max_detections_(max_detections),
        iou_threshold_(iou_threshold),
        detections_(std::make_unique<Detection[]>(max_detections)) {}

  bool decode(TfLiteTensor *const *outputs, size_t count) override;

  const Detection *detections() const { return this->detections_.get(); }
  size_t detection_count() const { return this->detection_count_; }

 protected:
  /// Insert in score order.
  void insert_(const Detection &detection);
  /// Drop a box that overlaps a higher-scored one of the same class.
  void suppress_();

  float score_threshold_;
  size_t max_detections_;
  float iou_threshold_;
  std::unique_ptr<Detection[]> detections_;
  size_t detection_count_{0};
};

}  // namespace esphome::tflite_detection

#endif  // USE_TFLITE_MICRO_HELPER
