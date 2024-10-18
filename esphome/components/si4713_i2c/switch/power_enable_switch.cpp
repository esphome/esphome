#include "power_enable_switch.h"

namespace esphome {
namespace si4713 {

void PowerEnableSwitch::write_state(bool value) {
  this->publish_state(value);
  this->parent_->set_power_enable(value);
}

}  // namespace si4713
}  // namespace esphome