#include "gate_threshold_number.h"

namespace esphome::ld2412 {

void GateThresholdNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_gate_threshold(this->motion_);
}

}  // namespace esphome::ld2412
