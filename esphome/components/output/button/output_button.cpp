#include "output_button.h"
#include "esphome/core/log.h"

namespace esphome::output {

ESPHOME_LOG_TAG(TAG, "output.button");

static constexpr uint32_t RESET_TIMEOUT_ID = 0;

void OutputButton::dump_config() {
  LOG_BUTTON("", "Output Button", this);
  ESP_LOGCONFIG(TAG, "  Duration: %.1fs", this->duration_ / 1e3f);
}
void OutputButton::press_action() {
  this->output_->turn_on();

  // One timeout id, so a second press restarts the reset instead of stacking another
  this->set_timeout(RESET_TIMEOUT_ID, this->duration_, [this]() { this->output_->turn_off(); });
}

}  // namespace esphome::output
