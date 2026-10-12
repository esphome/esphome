#include "sendspin_switch.h"

#if defined(USE_ESP_IDF) && defined(USE_SENDSPIN_SWITCH)

#include "esphome/core/log.h"
#include "esphome/core/preferences.h"

namespace esphome::sendspin_ {

ESPHOME_LOG_TAG(TAG, "sendspin.switch");

// --- SendspinEnabledSwitch ---

void SendspinEnabledSwitch::setup() {
  // The hub waits for this request, so a restore mode without a state still has to answer.
  this->control(this->get_initial_state_with_restore_mode().value_or(true));
}

void SendspinEnabledSwitch::dump_config() { LOG_SWITCH("", "Sendspin Enabled Switch", this); }

// THREAD CONTEXT: Main loop
void SendspinEnabledSwitch::write_state(bool state) {
  this->parent_->set_enabled(state);
  this->publish_state(state);
}

// --- SendspinUnpairedAccessSwitch ---

void SendspinUnpairedAccessSwitch::setup() {
  // The hub holds the client's first start for this, so a restore mode without a state still has to answer.
  this->control(this->get_initial_state_with_restore_mode().value_or(true));
}

void SendspinUnpairedAccessSwitch::dump_config() { LOG_SWITCH("", "Sendspin Unpaired Access Switch", this); }

// THREAD CONTEXT: Main loop
void SendspinUnpairedAccessSwitch::write_state(bool state) {
  const bool turning_off = !state && this->has_state() && this->state;
  this->parent_->set_unpaired_access_enabled(state);
  this->publish_state(state);
  // Write the change to flash now: losing it in a power cut would reopen unpaired access.
  if (turning_off) {
    global_preferences->sync();
  }
}

}  // namespace esphome::sendspin_

#endif
