#include "pa_bias_switch.h"

namespace esphome {
namespace kt0803 {

void PaBiasSwitch::write_state(bool value) {
  this->publish_state(value);
  this->parent_->set_pa_bias(value);
}

}  // namespace kt0803
}  // namespace esphome
