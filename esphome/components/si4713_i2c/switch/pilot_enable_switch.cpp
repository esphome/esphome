#include "pilot_enable_switch.h"

namespace esphome {
namespace si4713 {

void PilotEnableSwitch::write_state(bool value) {
  this->publish_state(value);
  this->parent_->set_pilot_enable(value);
}

}  // namespace si4713
}  // namespace esphome