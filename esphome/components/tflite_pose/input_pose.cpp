#include "input_pose.h"

#ifdef USE_TFLITE_MICRO_HELPER

#include "esphome/core/log.h"

namespace esphome::tflite_pose {

ESPHOME_LOG_TAG(TAG, "tflite_pose");

bool PoseDecoder::decode(TfLiteTensor *const *outputs, size_t count) {
  if (count != 1) {
    ESP_LOGE(TAG, "A pose model needs 1 output, this one has %zu", count);
    return false;
  }
  TfLiteTensor *tensor = outputs[0];
  if (tensor->dims == nullptr || tensor->dims->size != 4 || tensor->dims->data[0] != 1 || tensor->dims->data[1] != 1 ||
      tensor->dims->data[3] != 3) {
    ESP_LOGE(TAG, "The keypoint tensor is not [1, 1, K, 3]");
    return false;
  }
  const size_t keypoints = tensor->dims->data[2];
  if (keypoints == 0) {
    ESP_LOGE(TAG, "The keypoint tensor has no keypoints");
    return false;
  }
  if (this->count_ != keypoints) {
    this->keypoints_ = RAMAllocator<Keypoint>().make_unique_array_for_overwrite(keypoints);
    if (!this->keypoints_) {
      ESP_LOGE(TAG, "Could not allocate the %zu keypoints", keypoints);
      return false;
    }
    this->count_ = keypoints;
  }
  for (size_t index = 0; index < keypoints; index++) {
    float y;
    float x;
    float score;
    if (!tflite_output_processor::read_tensor_value(tensor, index * 3 + 0, &y) ||
        !tflite_output_processor::read_tensor_value(tensor, index * 3 + 1, &x) ||
        !tflite_output_processor::read_tensor_value(tensor, index * 3 + 2, &score)) {
      ESP_LOGE(TAG, "Keypoint %zu could not be read", index);
      return false;
    }
    this->keypoints_[index] = Keypoint{y, x, score};
  }
  return true;
}

}  // namespace esphome::tflite_pose

#endif  // USE_TFLITE_MICRO_HELPER
