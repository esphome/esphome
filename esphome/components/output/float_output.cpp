#include "float_output.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::output {

static const char *const TAG = "output.float";

#ifdef USE_OUTPUT_FLOAT_POWER_SCALING
void FloatOutput::set_max_power(float max_power) {
  this->max_power_ = clamp(max_power, this->min_power_, 1.0f);  // Clamp to min_power <= max <= 1.0
}

void FloatOutput::set_min_power(float min_power) {
  this->min_power_ = clamp(min_power, 0.0f, this->max_power_);  // Clamp to 0.0 <= min <= max_power
}
#endif

void FloatOutput::set_level(float state) {
  state = clamp(state, 0.0f, 1.0f);

#ifdef USE_POWER_SUPPLY
  if (state > 0.0f) {  // ON
    this->power_.request();
  } else {  // OFF
    this->power_.unrequest();
  }
#endif

#ifdef USE_OUTPUT_FLOAT_POWER_SCALING
  if (state != 0.0f || !this->zero_means_zero_)  // regardless of min_power_, 0.0 means off
    state = (state * (this->max_power_ - this->min_power_)) + this->min_power_;
#endif

  if (this->is_inverted())
    state = 1.0f - state;
  this->write_state(state);
}

void FloatOutput::write_state(bool state) { this->set_level(state != this->inverted_ ? 1.0f : 0.0f); }

}  // namespace esphome::output
