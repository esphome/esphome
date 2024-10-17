#include "xtal_sel_select.h"

namespace esphome {
namespace kt0803 {

void XtalSelSelect::control(const std::string &value) {
  this->publish_state(value);
  if (auto index = this->active_index()) {
    this->parent_->set_xtal_sel((XtalSel) *index);
  }
}

}  // namespace kt0803
}  // namespace esphome
