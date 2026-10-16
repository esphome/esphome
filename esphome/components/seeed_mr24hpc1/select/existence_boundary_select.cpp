#include "existence_boundary_select.h"

namespace esphome::seeed_mr24hpc1 {

void ExistenceBoundarySelect::control(size_t index) {
  this->publish_state(index);
  this->parent_->set_existence_boundary(index);
}

}  // namespace esphome::seeed_mr24hpc1
