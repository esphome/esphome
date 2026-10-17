#include "m5_unit_bldc_select.h"

namespace esphome::m5_unit_bldc {

void M5UnitBldcDirectionSelect::control(size_t index) {
  // Options are declared as ["Forward", "Backward"], so index 1 is Backward.
  Direction direction = index == 1 ? Direction::DIRECTION_BACKWARD : Direction::DIRECTION_FORWARD;
  if (this->parent_->write_direction(direction))
    this->publish_state(index);
}

}  // namespace esphome::m5_unit_bldc
