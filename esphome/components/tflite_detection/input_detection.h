#pragma once

#include "esphome/core/defines.h"

#include <cstddef>
#include <cstdint>

#ifdef USE_TFLITE_MICRO_HELPER
#include <memory>

#include "esphome/components/tflite_output_processor/output_processor.h"
#endif

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

/// The layout of the model's output.
enum class DetectionFormat : uint8_t {
  DETECTION_FORMAT_SSD,     // boxes, classes, scores and count, after the model's own post-processing
  DETECTION_FORMAT_YOLOV5,  // [1, candidates, 5 + classes]: center x, center y, width, height, objectness
  DETECTION_FORMAT_YOLOV8,  // [1, 4 + classes, candidates]: center x, center y, width, height
};

/// @brief Reads one raw YOLO candidate; false when its score is below the threshold.
///
/// The score is the best class score, multiplied by the objectness when the model has
/// one. Coordinates are normalized to the input frame, as TensorFlow Lite exports
/// report them, and the box is converted from center and size to its corners.
///
/// @param value  `value(field)` returns field @p field of this candidate.
template<typename Value>
bool yolo_candidate(Value &&value, size_t classes, bool has_objectness, float score_threshold, Detection &detection) {
  const float objectness = has_objectness ? value(4) : 1.0f;
  // Class scores are at most 1, so a low objectness settles it without reading them. Written to skip NaN.
  if (has_objectness && !(objectness >= score_threshold))
    return false;
  const size_t first_class = has_objectness ? 5 : 4;
  size_t best = 0;
  float best_score = value(first_class);
  for (size_t index = 1; index < classes; index++) {
    const float score = value(first_class + index);
    if (score > best_score) {
      best = index;
      best_score = score;
    }
  }
  detection.score = objectness * best_score;
  if (!(detection.score >= score_threshold))
    return false;
  const float center_x = value(0);
  const float center_y = value(1);
  const float half_width = value(2) * 0.5f;
  const float half_height = value(3) * 0.5f;
  detection.x_min = center_x - half_width;
  detection.x_max = center_x + half_width;
  detection.y_min = center_y - half_height;
  detection.y_max = center_y + half_height;
  detection.class_id = static_cast<int>(best);
  return true;
}

/// Whether a kept box of the same class overlaps `candidate` by more than `iou_threshold`.
bool suppresses(const Detection &kept, const Detection &candidate, float iou_threshold);

/// @brief Greedy non-maximum suppression in descending score order, then the cap.
///
/// `get(index, detection)` fills candidate `index` and returns false to skip it. Each
/// pass picks the best candidate after the previous pick that no kept box suppresses,
/// so only a box that is itself kept suppresses another, and no buffer for all the
/// candidates is needed.
///
/// @return the number of boxes written to `selected`, at most `max_detections`.
template<typename Get>
size_t select_detections(Detection *selected, size_t max_detections, float iou_threshold, size_t candidates,
                         Get &&get) {
  size_t count = 0;
  float last_score = 0.0f;
  size_t last_index = 0;
  while (count < max_detections) {
    bool found = false;
    Detection best;
    size_t best_index = 0;
    for (size_t index = 0; index < candidates; index++) {
      Detection candidate;
      if (!get(index, candidate))
        continue;
      // Candidates ordered before the previous pick were kept or suppressed already.
      if (count > 0 && !(candidate.score < last_score || (candidate.score == last_score && index > last_index)))
        continue;
      if (found && candidate.score <= best.score)
        continue;
      bool covered = false;
      for (size_t i = 0; i < count && !covered; i++)
        covered = suppresses(selected[i], candidate, iou_threshold);
      if (covered)
        continue;
      best = candidate;
      best_index = index;
      found = true;
    }
    if (!found)
      break;
    selected[count++] = best;
    last_score = best.score;
    last_index = best_index;
  }
  return count;
}

#ifdef USE_TFLITE_MICRO_HELPER
/// @brief Decodes a detection model's outputs: the four of SSD post-processing, or raw YOLO candidates.
class DetectionDecoder final : public tflite_output_processor::OutputDecoder {
 public:
  DetectionDecoder(float score_threshold, size_t max_detections, float iou_threshold,
                   DetectionFormat format = DetectionFormat::DETECTION_FORMAT_SSD)
      : score_threshold_(score_threshold),
        max_detections_(max_detections),
        iou_threshold_(iou_threshold),
        detections_(std::make_unique<Detection[]>(max_detections)),
        format_(format) {}

  bool decode(TfLiteTensor *const *outputs, size_t count) override;

  const Detection *detections() const { return this->detections_.get(); }
  size_t detection_count() const { return this->detection_count_; }

 protected:
  bool decode_yolo_(const TfLiteTensor *tensor);

  float score_threshold_;
  size_t max_detections_;
  float iou_threshold_;
  std::unique_ptr<Detection[]> detections_;
  size_t detection_count_{0};
  DetectionFormat format_;
};
#endif

}  // namespace esphome::tflite_detection
