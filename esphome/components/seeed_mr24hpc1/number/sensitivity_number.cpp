#include "sensitivity_number.h"

namespace esphome {
namespace mr24hpc1 {

void SensitivityNumber::control(float value) {
  this->parent_->set_sensitivity(value);
}

}  // namespace mr24hpc1
}  // namespace esphome
