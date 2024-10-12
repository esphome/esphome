#include "standby_enable_switch.h"

namespace esphome {
namespace kt0803 {

void StandbyEnableSwitch::write_state(bool value) {
  this->publish_state(value);
  this->parent_->set_standby_enable(value);
}

}  // namespace kt0803
}  // namespace esphome
