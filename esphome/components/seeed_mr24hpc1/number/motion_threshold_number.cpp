#include "motion_threshold_number.h"

namespace esphome {
namespace seeed_mr24hpc1 {

void MotionThresholdNumber::control(float value) { this->parent_->set_motion_threshold(value); }

}  // namespace seeed_mr24hpc1
}  // namespace esphome
