#include "hoermann_binary_sensor.h"

namespace esphome::hoermann {

void HoermannConnectedBinarySensor::setup() {
  this->parent_->add_on_state_callback([this]() { this->update_from_state_(); });
  this->publish_initial_state(false);
}
void HoermannConnectedBinarySensor::update_from_state_() {
  bool connected = this->parent_->is_valid();
  if (connected != this->state)
    this->publish_state(connected);
}

}  // namespace esphome::hoermann
