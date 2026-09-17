#include "sensitivity_select.h"

namespace esphome::ld2460 {

void SensitivitySelect::control(size_t index) {
  this->publish_state(index);
  this->parent_->set_sensitivity(this->option_at(index));
}

}  // namespace esphome::ld2460
