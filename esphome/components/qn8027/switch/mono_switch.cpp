#include "mono_switch.h"

namespace esphome {
namespace qn8027 {

void MonoSwitch::write_state(bool value) {
  this->publish_state(value);
  this->parent_->set_mono(value);
}

}  // namespace qn8027
}  // namespace esphome
