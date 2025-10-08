#include "multi_target_switch.h"

namespace esphome {
namespace ld2450 {

void MultiTargetSwitch::write_state(bool state) {
  this->publish_state(state);
  this->parent_->set_multi_target(state);
}

}  // namespace ld2450
}  // namespace esphome
