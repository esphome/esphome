#include "t1m_sel_select.h"

namespace esphome {
namespace qn8027 {

void T1mSelSelect::control(const std::string &value) {
  this->publish_state(value);
  if (auto index = this->active_index()) {
    this->parent_->set_t1m_sel((T1mSel) *index);
  }
}

}  // namespace qn8027
}  // namespace esphome
