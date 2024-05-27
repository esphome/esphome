#include "min_speed_threshold_number.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ld2415h {

static const char *const TAG = "LD2415H.min_speed_threshold_number";

void MinSpeedThresholdNumber::control(float speed) {
  this->publish_state(speed);
  this->parent_->set_min_speed_threshold(speed);
}

}  // namespace ld2415h
}  // namespace esphome
