#include "float_output.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace output {

static const char *const TAG = "output.float";

void FloatOutput::set_max_power(float max_power) {
  this->max_power_ = clamp(max_power, this->min_power_, 1.0f);  // Clamp to MIN>=MAX>=1.0
}

float FloatOutput::get_max_power() const { return this->max_power_; }

void FloatOutput::set_min_power(float min_power) {
  this->min_power_ = clamp(min_power, 0.0f, this->max_power_);  // Clamp to 0.0>=MIN>=MAX
}

void FloatOutput::set_zero_means_zero(bool zero_means_zero) { this->zero_means_zero_ = zero_means_zero; }

void FloatOutput::set_min_turn_on_power(float min_turn_on_power) { this->min_turn_on_power_ = min_turn_on_power; }

void FloatOutput::set_min_turn_on_power_delay(float min_turn_on_power_delay) {
  this->min_turn_on_power_delay_ = min_turn_on_power_delay;
}

float FloatOutput::get_min_power() const { return this->min_power_; }

bool FloatOutput::is_on_() {
  if (this->is_on_value_.has_value()) {
    return this->is_on_value_.value();
  } else {
    return false;
  }
}

void FloatOutput::set_level(float state) {
  state = clamp(state, 0.0f, 1.0f);

  const float state_orig = state;

#ifdef USE_POWER_SUPPLY
  if (state > 0.0f) {  // ON
    this->power_.request();
  } else {  // OFF
    this->power_.unrequest();
  }
#endif

  if (!(state == 0.0f && this->zero_means_zero_))  // regardless of min_power_, 0.0 means off
    state = (state * (this->max_power_ - this->min_power_)) + this->min_power_;

  if (this->is_inverted())
    state = 1.0f - state;

  if (!this->is_on_() && 0 < state && state < this->min_turn_on_power_) {
    ESP_LOGV(TAG, "FloatOutput is turned on and set to a low power (%f < %f), thus we set the min power (%f) first",
             state, this->min_turn_on_power_, this->min_turn_on_power_);
    this->write_state(this->is_inverted() ? 1.0f - this->min_turn_on_power_ : this->min_turn_on_power_);
    ESP_LOGV(TAG, "Staying at minimum power for %ums", this->min_turn_on_power_delay_);
    delay(this->min_turn_on_power_delay_);
    ESP_LOGV(TAG, "Now setting the actually requested power");
  }

  this->write_state(state);

  if (state_orig > 0.0f) {  // ON
    this->is_on_value_ = true;
  } else {  // OFF
    this->is_on_value_ = false;
  }
}

void FloatOutput::write_state(bool state) { this->set_level(state != this->inverted_ ? 1.0f : 0.0f); }

}  // namespace output
}  // namespace esphome
