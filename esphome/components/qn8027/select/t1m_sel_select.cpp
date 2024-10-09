#include "t1m_sel_select.h"

namespace esphome {
namespace qn8027 {

void T1mSelSelect::control(const std::string &value) {
  this->publish_state(value);
  this->parent_->set_t1m_sel(atoi(state.c_str()));
}

}  // namespace qn8027
}  // namespace esphome
