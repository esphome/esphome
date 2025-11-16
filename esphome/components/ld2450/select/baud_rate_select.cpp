#include "baud_rate_select.h"

namespace esphome {
namespace ld2450 {

void BaudRateSelect::control(size_t index) {
  this->publish_state(index);
  this->parent_->set_baud_rate(this->option_at(index));
}

}  // namespace ld2450
}  // namespace esphome
