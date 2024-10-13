#include "ref_clk_enable_switch.h"

namespace esphome {
namespace kt0803 {

void RefClkEnableSwitch::write_state(bool value) {
  this->publish_state(value);
  this->parent_->set_ref_clk_enable(value);
}

}  // namespace kt0803
}  // namespace esphome
