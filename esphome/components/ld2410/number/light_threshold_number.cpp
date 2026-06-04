#include "light_threshold_number.h"

namespace esphome::ld2410 {

void LightThresholdNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_light_out_control();
}

}  // namespace esphome::ld2410
