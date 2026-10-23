#include "opentherm_switch.h"

namespace esphome::opentherm {

ESPHOME_LOG_TAG(TAG, "opentherm.switch");

void OpenthermSwitch::write_state(bool state) { this->publish_state(state); }

void OpenthermSwitch::setup() {
  auto restored = this->get_initial_state_with_restore_mode();
  bool state = false;
  if (!restored.has_value()) {
    ESP_LOGD(TAG, "Couldn't restore state for OpenTherm switch '%s'", LOG_STR_ARG(this->get_log_name()));
  } else {
    ESP_LOGD(TAG, "Restored state for OpenTherm switch '%s': %d", LOG_STR_ARG(this->get_log_name()), restored.value());
    state = restored.value();
  }
  this->write_state(state);
}

void OpenthermSwitch::dump_config() {
  LOG_SWITCH("", "OpenTherm Switch", this);
  ESP_LOGCONFIG(TAG, "  Current state: %d", this->state);
}

}  // namespace esphome::opentherm
