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

void SigmaDeltaOutput::update() {
  this->accum_ += this->state_;
  const bool next_value = this->accum_ > 0;

  if (next_value) {
    this->accum_ -= 1.;
  }

  if (next_value != this->value_) {
    this->value_ = next_value;
    if (this->pin_) {
      this->pin_->digital_write(next_value);
    }

    if (this->state_change_trigger_) {
      this->state_change_trigger_->trigger(next_value);
    }

    if (next_value && this->turn_on_trigger_) {
      this->turn_on_trigger_->trigger();
    } else if (!next_value && this->turn_off_trigger_) {
      this->turn_off_trigger_->trigger();
    }
  }
}

}  // namespace sigma_delta_output
}  // namespace esphome
