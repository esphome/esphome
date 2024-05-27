#include "compensation_angle_number.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ld2415h {

static const char *const TAG = "LD2415H.compensation_angle_number";

void CompensationAngleNumber::control(float angle) {
  this->publish_state(angle);
  this->parent_->set_compensation_angle(angle);
}

}  // namespace ld2415h
}  // namespace esphome
