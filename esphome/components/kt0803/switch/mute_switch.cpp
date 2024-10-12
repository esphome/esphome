#include "mute_switch.h"

namespace esphome {
namespace kt0803 {

void MuteSwitch::write_state(bool value) {
  this->publish_state(value);
  this->parent_->set_mute(value);
}

}  // namespace kt0803
}  // namespace esphome
