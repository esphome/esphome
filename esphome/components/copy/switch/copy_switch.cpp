#include "copy_switch.h"
#include "esphome/core/log.h"

namespace esphome::copy {

ESPHOME_LOG_TAG(TAG, "copy.switch");

void CopySwitch::setup() {
  source_->add_on_state_callback([this](float value) { this->publish_state(value); });

  this->publish_state(source_->state);
}

void CopySwitch::dump_config() { LOG_SWITCH("", "Copy Switch", this); }

void CopySwitch::write_state(bool state) { this->source_->control(state); }

}  // namespace esphome::copy
