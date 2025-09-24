#include "engineering_mode_switch.h"

namespace esphome {
namespace ld2410 {

void EngineeringModeSwitch::write_state(bool state) {
  this->publish_state(state);
  this->parent_->set_engineering_mode(state);
}

}  // namespace ld2410
}  // namespace esphome
