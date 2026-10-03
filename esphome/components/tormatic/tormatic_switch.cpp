#include "tormatic_switch.h"

namespace esphome::tormatic {

void TormaticLightSwitch::write_state(bool state) {
  this->parent_->set_light_state(state);

  // No authoritative light status is currently available from the gate,
  // so treat the requested state as the current state.
  this->publish_state(state);
}

}  // namespace esphome::tormatic
