#include "output_switch.h"
#include "esphome/core/log.h"

namespace esphome::output {

static const char *const TAG = "output.switch";

void OutputSwitch::dump_config() { LOG_SWITCH("", "Output Switch", this); }
void OutputSwitch::setup() { this->control(this->get_initial_state_with_restore_mode().value_or(false)); }
void OutputSwitch::write_state(bool state) {
  if (state) {
    this->output_->turn_on();
  } else {
    this->output_->turn_off();
  }
  this->publish_state(state);
}

}  // namespace esphome::output
