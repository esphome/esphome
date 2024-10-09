#include "xtal_source_select.h"

namespace esphome {
namespace qn8027 {

void XtalSourceSelect::control(const std::string &value) {
  this->publish_state(value);
  if (auto index = this->active_index()) {
    this->parent_->set_xtal_source((XtalSource) *index);
  }
}

}  // namespace qn8027
}  // namespace esphome
