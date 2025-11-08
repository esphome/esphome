#include "unman_time_select.h"

namespace esphome {
namespace seeed_mr24hpc1 {

void UnmanTimeSelect::control(size_t index) {
  this->publish_state(index);
  this->parent_->set_unman_time(index);
}

}  // namespace seeed_mr24hpc1
}  // namespace esphome
