#include "direction.h"

namespace esphome::airton {

void VerticalDirectionSelect::control(const std::string &value) {
  if (this->parent_->get_vertical_direction_state().to_string() != value) {
    ESP_LOGD("vertical_direction", "Select received value: %s", value.c_str());
    this->parent_->set_vertical_direction_state(value);
  }
  this->publish_state(value);
}

}  // namespace esphome::airton
