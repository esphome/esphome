#include "rf_switch.h"

namespace esphome::at581x {

void RFSwitch::write_state(bool state) {
  this->publish_state(state);
  this->parent_->set_rf_mode(state);
}

}  // namespace esphome::at581x
