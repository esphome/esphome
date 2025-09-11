#include "c4001_switch.h"
#include "esphome/core/log.h"
namespace esphome {
namespace dfrobot_c4001 {

void C4001Switch::write_state(bool state) {
  if (this->parent_) {
    this->parent_->set_micro_switch_state(state);
    this->publish_state(state);
  }
}

}  // namespace dfrobot_c4001
}  // namespace esphome
