#include "output_button.h"
#include "esphome/core/log.h"

namespace esphome {
namespace output {

static const char *const TAG = "output.button";

void OutputButton::dump_config() {
  LOG_BUTTON("", "Output Button", this);
  ESP_LOGCONFIG(TAG, "  Duration: %.1fs", this->duration_ / 1e3f);
}
void OutputButton::press_action() {
  this->output_->turn_on();

  // Use a named timeout so that it's automatically cancelled if button is pressed again before it's reset
  this->set_timeout("reset", this->duration_, [this]() { this->output_->turn_off(); });
}

}  // namespace output
}  // namespace esphome
