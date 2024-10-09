#include "t1m_sel_number.h"

namespace esphome {
namespace qn8027 {

void T1mSelNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_t1m_sel((uint8_t) round(value));
}

}  // namespace qn8027
}  // namespace esphome
