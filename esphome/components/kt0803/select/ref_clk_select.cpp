#include "ref_clk_select.h"

namespace esphome {
namespace kt0803 {

void RefClkSelect::control(const std::string &value) {
  this->publish_state(value);
  if (auto index = this->active_index()) {
    this->parent_->set_ref_clk((ReferenceClock) *index);
  }
}

}  // namespace kt0803
}  // namespace esphome
