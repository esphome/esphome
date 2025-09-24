#include "copy_binary_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace copy {

static const char *const TAG = "copy.binary_sensor";

void CopyBinarySensor::setup() {
  source_->add_on_state_callback([this](bool value) { this->publish_state(value); });
  if (source_->has_state())
    this->publish_state(source_->state);
}

void CopyBinarySensor::dump_config() { LOG_BINARY_SENSOR("", "Copy Binary Sensor", this); }

}  // namespace copy
}  // namespace esphome
