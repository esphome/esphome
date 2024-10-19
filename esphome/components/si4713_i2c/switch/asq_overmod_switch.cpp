#include "asq_overmod_switch.h"

namespace esphome {
namespace si4713 {

void AsqOvermodSwitch::write_state(bool value) {
  this->publish_state(value);
  this->parent_->set_asq_overmod(value);
}

}  // namespace si4713
}  // namespace esphome