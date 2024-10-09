#include "priv_en_switch.h"

namespace esphome {
namespace qn8027 {

void PrivEnSwitch::write_state(bool value) {
  this->publish_state(value);
  this->parent_->set_priv_en(value);
}

}  // namespace qn8027
}  // namespace esphome
