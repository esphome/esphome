#include "asq_duration_high_number.h"

namespace esphome {
namespace si4713 {

void AsqDurationHighNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_asq_duration_high(value);
}

}  // namespace si4713
}  // namespace esphome