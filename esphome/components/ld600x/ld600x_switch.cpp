#include "ld600x_switch.h"

#ifdef USE_SWITCH

namespace esphome::ld600x {

void LD600XSwitch::write_state(bool state) {
  this->parent_->set_switch_state(this->kind_, state);
  this->publish_state(state);
}

}  // namespace esphome::ld600x

#endif  // USE_SWITCH
