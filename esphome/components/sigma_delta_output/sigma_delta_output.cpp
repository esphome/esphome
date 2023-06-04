#include "sigma_delta_output.h"
#include "esphome/core/log.h"

namespace esphome {
namespace sigma_delta_output {

static const char *const TAG = "output.sigma_delta";

void SigmaDeltaOutput::setup() {
  if (this->pin_)
    this->pin_->setup();
}

void SigmaDeltaOutput::dump_config() {
  ESP_LOGCONFIG(TAG, "Sigma Delta Output:");
  LOG_PIN("  Pin: ", this->pin_);
  if (this->state_change_trigger_) {
    ESP_LOGCONFIG(TAG, "  State change automation configured");
  }
  if (this->turn_on_trigger_) {
    ESP_LOGCONFIG(TAG, "  Turn on automation configured");
  }
  if (this->turn_off_trigger_) {
    ESP_LOGCONFIG(TAG, "  Turn off automation configured");
  }
  LOG_UPDATE_INTERVAL(this);
  LOG_FLOAT_OUTPUT(this);
}

}  // namespace sigma_delta_output
}  // namespace esphome
