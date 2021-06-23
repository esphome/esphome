#include "output_switch.h"
#include "esphome/core/log.h"

namespace esphome {
namespace output {

static const char *const TAG = "output.switch";

void OutputSwitch::dump_config() { LOG_SWITCH("", "Output Switch", this); }
void OutputSwitch::setup() {
  auto restored = this->get_initial_state();
  if (!restored.has_value())
    return;

  if (*restored) {
    this->turn_on();
  } else {
    this->turn_off();
  }
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
