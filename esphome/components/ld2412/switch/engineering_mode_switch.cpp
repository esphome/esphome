#include "engineering_mode_switch.h"

namespace esphome {
namespace ld2412 {

void EngineeringModeSwitch::write_state(bool state) {
  this->publish_state(state);
  this->parent_->set_engineering_mode(state);
}

}  // namespace ld2412
}  // namespace esphome
