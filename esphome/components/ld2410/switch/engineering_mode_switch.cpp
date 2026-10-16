#include "engineering_mode_switch.h"

namespace esphome::ld2410 {

void EngineeringModeSwitch::write_state(bool state) {
  this->publish_state(state);
  this->parent_->set_engineering_mode(state);
}

}  // namespace esphome::ld2410
