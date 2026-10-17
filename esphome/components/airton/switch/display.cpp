#include "display.h"

namespace esphome::airton {

void DisplaySwitch::write_state(bool state) {
  if (this->parent_->get_display_state() != state) {
    this->parent_->set_display_state(state, true);
  }
  this->publish_state(state);
}

}  // namespace esphome::airton
