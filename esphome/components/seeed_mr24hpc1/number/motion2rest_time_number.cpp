#include "motion2rest_time_number.h"

namespace esphome {
namespace seeed_mr24hpc1 {

void Motion2RestTimeNumber::control(float value) { this->parent_->set_motion_to_rest_time(value); }

}  // namespace seeed_mr24hpc1
}  // namespace esphome
