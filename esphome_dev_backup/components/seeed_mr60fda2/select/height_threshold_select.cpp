#include "height_threshold_select.h"

namespace esphome {
namespace seeed_mr60fda2 {

void HeightThresholdSelect::control(const std::string &value) {
  this->publish_state(value);
  auto index = this->index_of(value);
  if (index.has_value()) {
    this->parent_->set_height_threshold(index.value());
  }
}

}  // namespace seeed_mr60fda2
}  // namespace esphome
