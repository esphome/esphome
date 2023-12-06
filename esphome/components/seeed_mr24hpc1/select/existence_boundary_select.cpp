#include "existence_boundary_select.h"

namespace esphome {
namespace seeed_mr24hpc1 {

void ExistenceBoundarySelect::control(const std::string &value) { this->parent_->set_existence_boundary(value); }

}  // namespace seeed_mr24hpc1
}  // namespace esphome
