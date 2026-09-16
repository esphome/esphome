#include "sendspin_switch.h"

#ifdef USE_ESP32

#include "esphome/core/log.h"

namespace esphome::sendspin_ {

static const char *const TAG = "sendspin.switch";

void SendspinSwitch::setup() {
  // The hub waits for this request, so a restore mode without a state still has to answer.
  if (this->get_initial_state_with_restore_mode().value_or(false)) {
    this->turn_on();
  } else {
    this->turn_off();
  }
}

void SendspinSwitch::dump_config() { LOG_SWITCH("", "Sendspin Switch", this); }

// THREAD CONTEXT: Main loop
void SendspinSwitch::write_state(bool state) {
  this->parent_->set_enabled(state);
  this->publish_state(state);
}

}  // namespace esphome::sendspin_

#endif  // USE_ESP32
