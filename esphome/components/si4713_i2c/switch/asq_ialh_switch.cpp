#include "asq_ialh_switch.h"

namespace esphome {
namespace si4713 {

void AsqIalhSwitch::write_state(bool value) {
  this->publish_state(value);
  this->parent_->set_asq_ialh(value);
}

}  // namespace si4713
}  // namespace esphome