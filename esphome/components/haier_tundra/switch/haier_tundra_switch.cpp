#include "haier_tundra_switch.h"
#include "esphome/core/log.h"

namespace esphome::haier_tundra {

ESPHOME_LOG_TAG(TAG, "haier_tundra.switch");

void HaierTundraSwitch::setup() {
  auto initial = this->get_initial_state_with_restore_mode();
  if (initial.has_value()) {
    // apply_() and publish_state() take the level, which an inverted switch flips
    const bool level = *initial != this->is_inverted();
    this->apply_(level, false);
    this->publish_state(level);
  }
}

void HaierTundraSwitch::dump_config() { LOG_SWITCH("", "Haier Tundra Switch", this); }

void HaierTundraSwitch::write_state(bool state) {
  // Quiet and Turbo can only be turned on in Cool or Heat mode
  if (state && this->kind_ != SwitchKind::SWITCH_KIND_HEALTH && this->parent_->mode != climate::CLIMATE_MODE_COOL &&
      this->parent_->mode != climate::CLIMATE_MODE_HEAT) {
    return;
  }
  this->apply_(state, true);
  this->publish_state(state);
}

void HaierTundraSwitch::apply_(bool state, bool transmit) {
  switch (this->kind_) {
    case SwitchKind::SWITCH_KIND_HEALTH:
      this->parent_->set_health_mode(state, transmit);
      break;
    case SwitchKind::SWITCH_KIND_QUIET:
      this->parent_->set_quiet_mode(state, transmit);
      break;
    case SwitchKind::SWITCH_KIND_TURBO:
      this->parent_->set_turbo_mode(state, transmit);
      break;
  }
}

}  // namespace esphome::haier_tundra
