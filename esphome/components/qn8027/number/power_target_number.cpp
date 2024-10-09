#include "power_target_number.h"

namespace esphome {
namespace qn8027 {

void PowerTargetNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_power_target(value);
}

}  // namespace qn8027
}  // namespace esphome
