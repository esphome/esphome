#include "existence_threshold_number.h"

namespace esphome::seeed_mr24hpc1 {

void ExistenceThresholdNumber::control(float value) { this->parent_->set_existence_threshold(value); }

}  // namespace esphome::seeed_mr24hpc1
