#include "sensitivity_select.h"

namespace esphome::seeed_mr60fda2 {

void SensitivitySelect::control(size_t index) {
  this->publish_state(index);
  this->parent_->set_sensitivity(index);
}

}  // namespace esphome::seeed_mr60fda2
