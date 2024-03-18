#include "max_distance_timeout_number.h"

namespace esphome {
namespace ld2410 {

void MaxDistanceTimeoutNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_max_distances_timeout();
}

}  // namespace ld2410
}  // namespace esphome
