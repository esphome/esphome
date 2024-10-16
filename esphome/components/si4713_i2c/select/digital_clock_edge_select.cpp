#include "digital_clock_edge_select.h"

namespace esphome {
namespace si4713 {

void DigitalClockEdgeSelect::control(const std::string &value) {
  this->publish_state(value);
  if (auto index = this->active_index()) {
    this->parent_->set_digital_clock_edge((DigitalClockEdge) *index);
  }
}

}  // namespace si4713
}  // namespace esphome