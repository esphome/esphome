#include "refclk_source_select.h"

namespace esphome {
namespace si4713 {

void RefClkSourceSelect::control(const std::string &value) {
  this->publish_state(value);
  if (auto index = this->active_index()) {
    this->parent_->set_refclk_source((RefClkSource) *index);
  }
}

}  // namespace si4713
}  // namespace esphome