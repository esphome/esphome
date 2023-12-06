#include "motion_boundary_select.h"

namespace esphome {
namespace seeed_mr24hpc1 {

void MotionBoundarySelect::control(const std::string &value) { this->parent_->set_motion_boundary(value); }

}  // namespace seeed_mr24hpc1
}  // namespace esphome
