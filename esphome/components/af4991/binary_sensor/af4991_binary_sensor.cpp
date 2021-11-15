#include "af4991_binary_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace af4991 {

static const char *const TAG = "af4991.binary_sensor";

void AF4991BinarySensor::setup() {
  ESP_LOGCONFIG(TAG, "Setting up AF4991 Encoder Switch...");
  this->parent_->pin_mode(this->switch_pin_, adafruit_seesaw::INPUT_PULLUP);
}

void AF4991BinarySensor::dump_config() {
  LOG_BINARY_SENSOR("", "AF4991 Switch", this);
  ESP_LOGCONFIG(TAG, "  Pin: %i", this->switch_pin_);
}

void AF4991BinarySensor::loop() {
  if (!this->parent_->digital_read(this->switch_pin_)) {
    this->publish_state(true);
  } else {
    this->publish_state(false);
  }
}

}  // namespace af4991
}  // namespace esphome
