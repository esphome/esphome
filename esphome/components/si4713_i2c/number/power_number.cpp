#include "power_number.h"

namespace esphome {
namespace si4713 {

void PowerNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_power(value);
}

}  // namespace si4713
}  // namespace esphome