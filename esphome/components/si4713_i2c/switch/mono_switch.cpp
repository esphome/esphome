#include "mono_switch.h"

namespace esphome {
namespace si4713 {

void MonoSwitch::write_state(bool value) {
  this->publish_state(value);
  this->parent_->set_mono(value);
}

}  // namespace si4713
}  // namespace esphome
