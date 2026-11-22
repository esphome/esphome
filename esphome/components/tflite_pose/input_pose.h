#pragma once

#include "esphome/core/defines.h"

#ifdef USE_TFLITE_MICRO_HELPER

#include <cstddef>
#include <cstdint>
#include <memory>

#include "esphome/components/tflite_output_processor/output_processor.h"

namespace esphome::tflite_pose {

/// @brief One keypoint: position in the model's normalized coordinates and its score.
struct Keypoint {
  float y{0.0f};
  float x{0.0f};
  float score{0.0f};
};

/// @brief Decodes a [1, 1, K, 3] keypoint tensor of (y, x, score) triplets.
class PoseDecoder final : public tflite_output_processor::OutputDecoder {
 public:
  bool decode(TfLiteTensor *const *outputs, size_t count) override;

  const Keypoint *keypoints() const { return this->keypoints_.get(); }
  size_t keypoint_count() const { return this->count_; }

 protected:
  std::unique_ptr<Keypoint[]> keypoints_;
  size_t count_{0};
};

}  // namespace esphome::tflite_pose

#endif  // USE_TFLITE_MICRO_HELPER
