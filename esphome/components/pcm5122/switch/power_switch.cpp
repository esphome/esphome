#include "power_switch.h"

namespace esphome::pcm5122 {

void PCM5122PowerSwitch::write_state(bool state) {
  this->publish_state(state);
  if (this->mode_ == PCM5122_POWER_SWITCH_MODE_STANDBY) {
    this->parent_->set_standby(state);
  } else {
    this->parent_->set_powerdown(state);
  }
}

}  // namespace esphome::pcm5122
