#include "dangerous_controls.h"

namespace esphome::duco {

void DucoDangerousControls::setup() { this->publish_state(this->parent_->allows_dangerous_controls()); }

void DucoDangerousControls::write_state(bool state) {
  this->parent_->set_allow_dangerous_controls(state);
  this->publish_state(state);
}

}  // namespace esphome::duco
