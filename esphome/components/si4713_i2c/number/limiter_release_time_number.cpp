#include "limiter_release_time_number.h"

namespace esphome {
namespace si4713 {

void LimiterReleaseTimeNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_limiter_release_time(value);
}

}  // namespace si4713
}  // namespace esphome