#include "baud_rate_select.h"

namespace esphome::ld2460 {

void BaudRateSelect::control(size_t index) {
  this->publish_state(index);
  this->parent_->set_baud_rate(this->at(index)->c_str());
}

}  // namespace esphome::ld2460
