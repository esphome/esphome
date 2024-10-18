#include "alc_enable_switch.h"

namespace esphome {
namespace kt0803 {

void AlcEnableSwitch::write_state(bool value) {
  this->publish_state(value);
  this->parent_->set_alc_enable(value);
}

}  // namespace kt0803
}  // namespace esphome
