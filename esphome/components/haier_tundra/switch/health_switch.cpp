#include "health_switch.h"
#include "esphome/core/log.h"

namespace esphome::haier_tundra {

ESPHOME_LOG_TAG(TAG, "haier_tundra.switch.health");

void HealthSwitch::setup() {
  auto initial = this->get_initial_state_with_restore_mode();
  if (initial.has_value()) {
    this->write_state(*initial);
  }
}

void HealthSwitch::dump_config() { LOG_SWITCH("", "Health Mode Switch", this); }

void HealthSwitch::write_state(bool state) {
  this->parent_->set_health_mode(state);
  this->publish_state(state);
}

}  // namespace esphome::haier_tundra
