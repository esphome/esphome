#include "mono_switch.h"

namespace esphome {
namespace kt0803 {

void MonoSwitch::write_state(bool value) {
  this->publish_state(value);
  this->parent_->set_mono(value);
}

}  // namespace kt0803
}  // namespace esphome
