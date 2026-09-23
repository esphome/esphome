#include "number_sensor.h"
#include "esphome/core/log.h"

namespace esphome::number {

static const char *const TAG = "number.sensor";

void NumberSensor::setup() {
  this->source_->add_on_state_callback([this](float value) { this->publish_state(value); });
  if (this->source_->has_state())
    this->publish_state(this->source_->state);
}

void NumberSensor::dump_config() { LOG_SENSOR("", "Number Sensor", this); }

}  // namespace esphome::number
