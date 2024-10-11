#include "sensitivity_number.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ld2415h {

static const char *const TAG = "LD2415H.sensitivity_number";

void SensitivityNumber::control(float sensitivity) {
  this->publish_state(sensitivity);
  this->parent_->set_sensitivity(sensitivity);
}

}  // namespace ld2415h
}  // namespace esphome
