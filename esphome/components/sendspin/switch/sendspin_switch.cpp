#include "sendspin_switch.h"

#ifdef USE_ESP32

#include "esphome/core/log.h"

namespace esphome::sendspin_ {

static const char *const TAG = "sendspin.switch";

void SendspinSwitch::setup() {
  auto initial_state = this->get_initial_state_with_restore_mode();
  if (initial_state.has_value()) {
    this->write_state(*initial_state);
  } else {
    this->publish_state(this->parent_->is_enabled());
  }
}

void SendspinSwitch::dump_config() { LOG_SWITCH("", "Sendspin Switch", this); }

// THREAD CONTEXT: Main loop
void SendspinSwitch::write_state(bool state) {
  this->parent_->set_enabled(state);
  // Publish what the hub actually did rather than what was asked, so a failed start reads as off.
  this->publish_state(this->parent_->is_enabled());
}

}  // namespace esphome::sendspin_

#endif  // USE_ESP32
