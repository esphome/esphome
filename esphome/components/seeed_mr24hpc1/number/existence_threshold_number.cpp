#include "existence_threshold_number.h"

namespace esphome {
namespace mr24hpc1 {

void ExistenceThresholdNumber::control(float value) {
  this->parent_->set_existence_threshold(value);
}

}  // namespace mr24hpc1
}  // namespace esphome
