#include "ld600x_select.h"

#ifdef USE_SELECT

namespace esphome::ld600x {

void LD600XSelect::control(size_t index) {
  this->publish_state(index);
  this->parent_->set_select_value(this->kind_, index);
}

}  // namespace esphome::ld600x

#endif  // USE_SELECT
