#include "esphome/core/log.h"

#include "ds3231_sensor_component.h"

namespace esphome {
namespace ds3231 {

static const char *TAG = "ds3231.sensor";

void DS3231SensorComponent::update() {
  uint8_t raw[2] = {};
  this->read_bytes(0x11, raw, 2);

  int8_t nint;
  if ((raw[0] & 0x80) != 0)
    nint = raw[0] | ~((1 << 8) - 1);  // if negative get two's complement
  else
    nint = raw[0];

  float temperature = 0.25 * (raw[1] >> 6) + nint;
  if (this->temperature_sensor_ != nullptr)
    this->temperature_sensor_->publish_state(temperature);
}

}  // namespace ds3231
}  // namespace esphome
