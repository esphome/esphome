#include "esphome/core/helpers.h"  // for clamp() and lerp()
#include "opentherm_output.h"

namespace esphome {
namespace opentherm {

static const char *const TAG = "opentherm.output";

void opentherm::OpenthermOutput::write_state(float state) {
  ESP_LOGD(TAG, "Received state: %.2f. Min value: %.2f, max value: %.2f", state, min_value_, max_value_);
  this->state = state < 0.003 && this->zero_means_zero_
                    ? 0.0
                    : clamp(std::lerp(min_value_, max_value_, state), min_value_, max_value_);
  this->has_state_ = true;
  ESP_LOGD(TAG, "Output %s set to %.2f", this->id_, this->state);
}
}  // namespace opentherm
}  // namespace esphome
