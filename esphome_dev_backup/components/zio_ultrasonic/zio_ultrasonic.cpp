
#include "zio_ultrasonic.h"

#include "esphome/core/log.h"

namespace esphome {
namespace zio_ultrasonic {

static const char *const TAG = "zio_ultrasonic";

void ZioUltrasonicComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Zio Ultrasonic Sensor:");
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Sensor:", this);
}

void ZioUltrasonicComponent::update() {
  uint16_t distance;

  // Read an unsigned two byte integerfrom register 0x01 which gives distance in mm
  if (!this->read_byte_16(0x01, &distance)) {
    ESP_LOGE(TAG, "Error reading data from Zio Ultrasonic");
    this->publish_state(NAN);
  } else {
    this->publish_state(distance);
  }
}

}  // namespace zio_ultrasonic
}  // namespace esphome
