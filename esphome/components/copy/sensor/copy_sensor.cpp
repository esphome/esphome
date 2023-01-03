#include "copy_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace copy {

static const char *const TAG = "copy.sensor";

void CopySensor::setup() {
  source_->add_on_state_callback([this](float value) { this->publish_state(value); });
  if (source_->has_state())
    this->publish_state(source_->state);
}

void CopySensor::dump_config() { LOG_SENSOR("", "Copy Sensor", this); }

}  // namespace copy
}  // namespace esphome
