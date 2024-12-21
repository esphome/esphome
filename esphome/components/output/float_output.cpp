#include "float_output.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

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

float FloatOutput::get_min_power() const { return this->min_power_; }

void FloatOutput::set_level(float state) {
  state = clamp(state, 0.0f, 1.0f);

#ifdef USE_POWER_SUPPLY
  if (state > 0.0f) {  // ON
    this->power_.request();
  } else {  // OFF
    this->power_.unrequest();
  }
#endif

  if (state != 0.0f || !this->zero_means_zero_)  // regardless of min_power_, 0.0 means off
    state = (state * (this->max_power_ - this->min_power_)) + this->min_power_;

  if (this->is_inverted())
    state = 1.0f - state;
  this->write_state(state);
}

void FloatOutput::write_state(bool state) { this->set_level(state != this->inverted_ ? 1.0f : 0.0f); }

}  // namespace output
}  // namespace esphome
