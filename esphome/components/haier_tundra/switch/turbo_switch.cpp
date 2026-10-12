#include "turbo_switch.h"
#include "esphome/core/log.h"

namespace esphome::haier_tundra {

ESPHOME_LOG_TAG(TAG, "haier_tundra.switch.turbo");

void TurboSwitch::setup() {
  auto initial = this->get_initial_state_with_restore_mode();
  if (initial.has_value()) {
    this->write_state(*initial);
  }
}

void TurboSwitch::dump_config() { LOG_SWITCH("", "Turbo Mode Switch", this); }

void TurboSwitch::write_state(bool state) {
  // Not available in Heat/Cool, Dry or Fan modes
  if (!state || this->parent_->mode == climate::CLIMATE_MODE_COOL ||
      this->parent_->mode == climate::CLIMATE_MODE_HEAT) {
    this->parent_->set_turbo_mode(state);
    this->publish_state(state);
  }
}

}  // namespace esphome::haier_tundra
