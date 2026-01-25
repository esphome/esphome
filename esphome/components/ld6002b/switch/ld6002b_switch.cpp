#include "ld6002b_switch.h"

namespace esphome::ld6002b {

void LD6002BSwitch::write_state(bool state) {
  this->publish_state(state);
  this->parent_->set_switch_state(this->type_, state);
}

}  // namespace esphome::ld6002b
