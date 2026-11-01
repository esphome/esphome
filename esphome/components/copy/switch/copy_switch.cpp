#include "copy_switch.h"
#include "esphome/core/log.h"

namespace esphome::copy {

ESPHOME_LOG_TAG(TAG, "copy.switch");

void CopySwitch::setup() {
  this->source_switch_->add_on_state_callback([this](float value) { this->publish_state(value); });

  this->publish_state(this->source_switch_->state);
}

void CopySwitch::dump_config() { LOG_SWITCH("", "Copy Switch", this); }

void CopySwitch::write_state(bool state) { this->source_switch_->control(state); }

}  // namespace esphome::copy
