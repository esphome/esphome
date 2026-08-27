#include "motion_trigger_time_number.h"

namespace esphome::seeed_mr24hpc1 {

void MotionTriggerTimeNumber::control(float value) { this->parent_->set_motion_trigger_time(value); }

}  // namespace esphome::seeed_mr24hpc1
