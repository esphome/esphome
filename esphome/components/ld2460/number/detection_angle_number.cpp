#include "detection_angle_number.h"

namespace esphome::ld2460 {

void DetectionAngleMinNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_detection_range(this->parent_->get_detection_distance(), value,
                                     this->parent_->get_detection_angle_max());
}

void DetectionAngleMaxNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_detection_range(this->parent_->get_detection_distance(), this->parent_->get_detection_angle_min(),
                                     value);
}

}  // namespace esphome::ld2460
