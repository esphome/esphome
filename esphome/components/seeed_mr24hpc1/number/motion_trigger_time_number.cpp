#include "motion_trigger_time_number.h"

namespace esphome {
namespace mr24hpc1 {

void MotionTriggerTimeNumber::control(float value) {
  this->parent_->set_motion_trigger_time(value);
}

}  // namespace mr24hpc1
}  // namespace esphome
