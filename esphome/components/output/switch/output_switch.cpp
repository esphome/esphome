#include "output_switch.h"
#include "esphome/core/log.h"

namespace esphome::output {

ESPHOME_LOG_TAG(TAG, "output.switch");

void OutputSwitch::dump_config() { LOG_SWITCH("", "Output Switch", this); }
void OutputSwitch::setup() { this->restore(false); }
void OutputSwitch::write_state(bool state) {
  this->output_->set_state(state);
  this->publish_state(state);
}

}  // namespace esphome::output
