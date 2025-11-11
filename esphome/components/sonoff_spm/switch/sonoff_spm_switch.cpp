#include "sonoff_spm_switch.h"
#include "esphome/core/log.h"

namespace esphome {
namespace sonoff_spm {

static const char *const TAG = "sonoff_spm.switch";

void SonoffSPMSwitch::setup() {
  // Get initial state from parent
  if (this->parent_ != nullptr) {
    bool state = this->parent_->get_relay_state(this->relay_id_);
    this->publish_state(state);
  }
}

void SonoffSPMSwitch::dump_config() {
  LOG_SWITCH("", "Sonoff SPM Switch", this);
  ESP_LOGCONFIG(TAG, "  Relay ID: %d", this->relay_id_);
}

void SonoffSPMSwitch::write_state(bool state) {
  if (this->parent_ != nullptr) {
    this->parent_->set_relay_state(this->relay_id_, state);
    // Publish state immediately (will be confirmed by device response)
    this->publish_state(state);
  }
}

}  // namespace sonoff_spm
}  // namespace esphome
