#include "ultrasonic_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ultrasonic_uart {

static const char *const TAG = "ultrasonic_uart";

void UltrasonicSensorComponent_UART::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Ultrasonic Sensor...");
}
void UltrasonicSensorComponent_UART::update() {
  this->write(0x55);
  while (this->available() == 4) {
    auto frame = *this->read_array<4>();
    if ((frame[0] == 0xFF) && (((frame[0] + frame[1] + frame[2]) & 0x00FF) == frame[3])) {
      float value = ((frame[1] << 8) + frame[2]) / 10.0;
      this->publish_state(value);
    }
  }
}
void UltrasonicSensorComponent_UART::dump_config() {
  LOG_SENSOR("", "Ultrasonic Sensor", this);
  LOG_UPDATE_INTERVAL(this);
}
float UltrasonicSensorComponent_UART::get_setup_priority() const { return setup_priority::DATA; }
}  // namespace ultrasonic_uart
}  // namespace esphome
