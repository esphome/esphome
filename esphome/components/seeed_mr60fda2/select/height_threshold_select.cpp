#include "height_threshold_select.h"

namespace esphome {
namespace seeed_mr60fda2 {

void HeightThresholdSelect::control(size_t index) {
  this->publish_state(index);
  this->parent_->set_height_threshold(index);
}

}  // namespace seeed_mr60fda2
}  // namespace esphome
