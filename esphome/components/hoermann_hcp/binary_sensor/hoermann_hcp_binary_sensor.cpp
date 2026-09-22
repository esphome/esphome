#include "hoermann_hcp_binary_sensor.h"

#include "esphome/core/log.h"

namespace esphome::hoermann_hcp {

static const char *const TAG = "hoermann_hcp.binary_sensor";

void HoermannHcpConnectedBinarySensor::setup() {
  // Publishing unconditionally is deliberate: the base class dedupes, and filters need every input to drive
  // their timers.
  this->parent_->add_on_state_callback([this]() { this->publish_state(this->parent_->is_valid()); });
  this->publish_initial_state(this->parent_->is_valid());
}

void HoermannHcpConnectedBinarySensor::dump_config() { LOG_BINARY_SENSOR("", "Hoermann HCP Connected", this); }

}  // namespace esphome::hoermann_hcp
