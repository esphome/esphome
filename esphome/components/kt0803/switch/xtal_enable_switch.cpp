#include "xtal_enable_switch.h"

namespace esphome {
namespace kt0803 {

void XtalEnableSwitch::write_state(bool value) {
  this->publish_state(value);
  this->parent_->set_xtal_enable(value);
}

}  // namespace kt0803
}  // namespace esphome
