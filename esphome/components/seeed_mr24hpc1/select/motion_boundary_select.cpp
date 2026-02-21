#include "motion_boundary_select.h"

namespace esphome {
namespace seeed_mr24hpc1 {

void MotionBoundarySelect::control(size_t index) {
  this->publish_state(index);
  this->parent_->set_motion_boundary(index);
}

}  // namespace seeed_mr24hpc1
}  // namespace esphome
