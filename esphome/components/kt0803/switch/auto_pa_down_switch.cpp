#include "auto_pa_down_switch.h"

namespace esphome {
namespace kt0803 {

void AutoPaDownSwitch::write_state(bool value) {
  this->publish_state(value);
  this->parent_->set_auto_pa_down(value);
}

}  // namespace kt0803
}  // namespace esphome
