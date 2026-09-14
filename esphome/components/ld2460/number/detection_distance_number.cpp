#include "detection_distance_number.h"

namespace esphome::ld2460 {

void DetectionDistanceNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_detection_range(value, this->parent_->get_detection_angle_min(),
                                     this->parent_->get_detection_angle_max());
}

}  // namespace esphome::ld2460
