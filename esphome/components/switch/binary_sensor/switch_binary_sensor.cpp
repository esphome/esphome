#include "switch_binary_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace switch_ {

static const char *const TAG = "switch.binary_sensor";

void SwitchBinarySensor::setup() {
  source_->add_on_state_callback([this](bool value) { this->publish_state(value); });
  this->publish_state(source_->state);
}

void SwitchBinarySensor::dump_config() { LOG_BINARY_SENSOR("", "Switch Binary Sensor", this); }

}  // namespace switch_
}  // namespace esphome
