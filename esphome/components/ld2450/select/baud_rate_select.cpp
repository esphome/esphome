#include "baud_rate_select.h"

namespace esphome::ld2450 {

void BaudRateSelect::control(size_t index) {
  this->publish_state(index);
  this->parent_->set_baud_rate(static_cast<BaudRate>(index + 1));
}

}  // namespace esphome::ld2450
