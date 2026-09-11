#include "opentherm42_switch.h"

namespace esphome::opentherm42 {

static const char *const TAG = "opentherm42.switch";

void OpenTherm42Switch::write_state(bool state) { this->publish_state(state); }

void OpenTherm42Switch::setup() {
  // §4.2.6/§5.1: a write-only value has nothing to read back on boot, so the config's restore_mode
  // decides what gets sent to the boiler until the user changes it -- never "unknown".
  auto restored = this->get_initial_state_with_restore_mode();
  this->write_state(restored.has_value() && restored.value());
}

void OpenTherm42Switch::dump_config() { LOG_SWITCH("", "OpenTherm 4.2 Switch", this); }

}  // namespace esphome::opentherm42
