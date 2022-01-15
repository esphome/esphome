#include "output_switch.h"
#include "esphome/core/log.h"

namespace esphome {
namespace output {

static const char *const TAG = "output.switch";

void OutputSwitch::dump_config() { LOG_SWITCH("", "Output Switch", this); }
void OutputSwitch::setup() {
  bool initial_state = false;
  switch (this->restore_mode_) {
    case OUTPUT_SWITCH_RESTORE_DEFAULT_OFF:
      initial_state = this->get_initial_state().value_or(false);
      break;
    case OUTPUT_SWITCH_RESTORE_DEFAULT_ON:
      initial_state = this->get_initial_state().value_or(true);
      break;
    case OUTPUT_SWITCH_RESTORE_INVERTED_DEFAULT_OFF:
      initial_state = !this->get_initial_state().value_or(true);
      break;
    case OUTPUT_SWITCH_RESTORE_INVERTED_DEFAULT_ON:
      initial_state = !this->get_initial_state().value_or(false);
      break;
    case OUTPUT_SWITCH_ALWAYS_OFF:
      initial_state = false;
      break;
    case OUTPUT_SWITCH_ALWAYS_ON:
      initial_state = true;
      break;
  }

  if (initial_state)
    this->turn_on();
  else
    this->turn_off();
}
void OutputSwitch::write_state(bool state) {
  if (state) {
    this->output_->turn_on();
  } else {
    this->output_->turn_off();
  }
  this->publish_state(state);
}

}  // namespace output
}  // namespace esphome
