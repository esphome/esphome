#include "asq_ialh_enable_switch.h"

namespace esphome {
namespace si4713 {

void AsqIalhEnableSwitch::write_state(bool value) {
  this->publish_state(value);
  this->parent_->set_asq_ialh_enable(value);
}

}  // namespace si4713
}  // namespace esphome