#include "max_distance_timeout_number.h"

namespace esphome::ld2412 {

void MaxDistanceTimeoutNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_basic_config();
}

}  // namespace esphome::ld2412
