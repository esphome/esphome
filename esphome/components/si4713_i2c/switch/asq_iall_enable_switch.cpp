#include "asq_iall_enable_switch.h"

namespace esphome {
namespace si4713 {

void AsqIallEnableSwitch::write_state(bool value) {
  this->publish_state(value);
  this->parent_->set_asq_iall_enable(value);
}

}  // namespace si4713
}  // namespace esphome