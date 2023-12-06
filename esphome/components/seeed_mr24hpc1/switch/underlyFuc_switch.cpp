#include "underlyFuc_switch.h"

namespace esphome {
namespace seeed_mr24hpc1 {

void UnderlyOpenFunctionSwitch::write_state(bool state) {
  this->publish_state(state);
  this->parent_->set_underlying_open_function(state);
}

}  // namespace seeed_mr24hpc1
}  // namespace esphome
