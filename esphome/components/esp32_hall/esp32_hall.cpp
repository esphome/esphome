#include "esp32_hall.h"
#include "esphome/core/log.h"
#include "esphome/core/esphal.h"

#ifdef ARDUINO_ARCH_ESP32

namespace esphome {
namespace esp32_hall {

static const char *const TAG = "esp32_hall";

void ESP32HallSensor::update() {
  float value = (hallRead() / 4095.0f) * 10000.0f;
  ESP_LOGD(TAG, "'%s': Got reading %.0f µT", this->name_.c_str(), value);
  this->publish_state(value);
}
std::string ESP32HallSensor::unique_id() { return get_mac_address() + "-hall"; }
void ESP32HallSensor::dump_config() { LOG_SENSOR("", "ESP32 Hall Sensor", this); }

}  // namespace esp32_hall
}  // namespace esphome

#endif
