#include "motion_trigger_time_number.h"

namespace esphome {
namespace seeed_mr24hpc1 {

void MotionTriggerTimeNumber::control(float value) { this->parent_->set_motion_trigger_time(value); }

}  // namespace seeed_mr24hpc1
}  // namespace esphome
