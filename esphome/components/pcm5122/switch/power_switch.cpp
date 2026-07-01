#include "power_switch.h"

namespace esphome::pcm5122 {

void PCM5122PowerSwitch::write_state(bool state) {
  bool ok = (this->mode_ == PCM5122_POWER_SWITCH_MODE_STANDBY) ? this->parent_->set_standby(state)
                                                               : this->parent_->set_powerdown(state);
  if (ok)
    this->publish_state(state);
}

}  // namespace esphome::pcm5122
