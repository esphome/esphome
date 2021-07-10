#include "gpio_binary_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace gpio {

static const char *const TAG = "gpio.binary_sensor";

void GPIOBinarySensor::setup() {
  this->pin_->setup();
  this->publish_initial_state(this->pin_->digital_read());
}

void GPIOBinarySensor::dump_config() {
  LOG_BINARY_SENSOR("", "GPIO Binary Sensor", this);
  LOG_PIN("  Pin: ", this->pin_);
}

void GPIOBinarySensor::loop() { this->publish_state(this->pin_->digital_read()); }

float GPIOBinarySensor::get_setup_priority() const { return setup_priority::HARDWARE; }

}  // namespace gpio
}  // namespace esphome
