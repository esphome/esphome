#include "ld6002b_switch.h"

namespace esphome::ld6002b {

void LD6002BSwitch::write_state(bool state) {
  this->parent_->set_switch_state(this->type_, state);
  this->publish_state(state);
}

}  // namespace esphome::ld6002b
