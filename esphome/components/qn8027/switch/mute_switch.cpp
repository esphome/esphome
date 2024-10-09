#include "mute_switch.h"

namespace esphome {
namespace qn8027 {

void MuteSwitch::write_state(bool value) {
  this->publish_state(value);
  this->parent_->set_mute(value);
}

}  // namespace qn8027
}  // namespace esphome
