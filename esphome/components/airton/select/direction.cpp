#include "direction.h"

namespace esphome::airton {

// The options are in the order of the enum, so the index is the direction; the parent publishes the state
void VerticalDirectionSelect::control(const std::string &value) {
  if (auto index = this->index_of(value); index.has_value()) {
    this->parent_->set_vertical_direction_state(static_cast<VerticalDirection>(*index), true);
  }
}

}  // namespace esphome::airton
