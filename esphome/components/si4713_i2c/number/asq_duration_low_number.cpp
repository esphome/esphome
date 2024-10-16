#include "asq_duration_low_number.h"

namespace esphome {
namespace si4713 {

void AsqDurationLowNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_asq_duration_low(value);
}

}  // namespace si4713
}  // namespace esphome