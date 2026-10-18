#include "baud_rate_select.h"

namespace esphome::ld2412 {

void BaudRateSelect::control(size_t index) {
  this->publish_state(index);
  this->parent_->set_baud_rate(this->option_at(index));
}

}  // namespace esphome::ld2412
