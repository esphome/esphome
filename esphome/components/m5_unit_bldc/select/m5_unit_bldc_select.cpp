#include "m5_unit_bldc_select.h"

namespace esphome::m5_unit_bldc {

void M5UnitBldcDirectionSelect::control(const std::string &value) {
  this->publish_state(value);
  this->parent_->write_direction(value == "Backward" ? Direction::BACKWARD : Direction::FORWARD);
}

}  // namespace esphome::m5_unit_bldc
