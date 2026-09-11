#include "sleep.h"

namespace esphome::airton {

void SleepSwitch::write_state(bool state) {
  if (this->parent_->get_sleep_mode_state() != state) {
    this->parent_->set_sleep_mode_state(state, true);
  }
  this->publish_state(state);
}

}  // namespace esphome::airton
