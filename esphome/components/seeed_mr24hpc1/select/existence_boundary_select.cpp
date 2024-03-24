#include "existence_boundary_select.h"

namespace esphome {
namespace seeed_mr24hpc1 {

void ExistenceBoundarySelect::control(const std::string &value) {
  this->publish_state(value);
  auto index = this->index_of(value);
  if (index.has_value()) {
    this->parent_->set_existence_boundary(index.value());
  }
}

}  // namespace seeed_mr24hpc1
}  // namespace esphome
