#include "asq_iall_switch.h"

namespace esphome {
namespace si4713 {

void AsqIallSwitch::write_state(bool value) {
  this->publish_state(value);
  this->parent_->set_asq_iall(value);
}

}  // namespace si4713
}  // namespace esphome