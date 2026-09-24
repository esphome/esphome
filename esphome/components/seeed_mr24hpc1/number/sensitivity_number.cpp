#include "sensitivity_number.h"

namespace esphome::seeed_mr24hpc1 {

void SensitivityNumber::control(float value) { this->parent_->set_sensitivity(value); }

}  // namespace esphome::seeed_mr24hpc1
