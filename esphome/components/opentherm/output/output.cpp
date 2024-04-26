#include "output.h"

namespace esphome {
namespace opentherm {

void opentherm::OpenthermOutput::write_state(float state) {
  ESP_LOGD("opentherm.output", "Received state: %.2f. Min value: %.2f, max value: %.2f", state, min_value, max_value);
  this->state = state < 0.003 && this->zero_means_zero_ ? 0.0 : min_value + state * (max_value - min_value);
  this->has_state_ = true;
  ESP_LOGD("opentherm.output", "Output %s set to %.2f", this->id, this->state);
}
}  // namespace opentherm
}  // namespace esphome
