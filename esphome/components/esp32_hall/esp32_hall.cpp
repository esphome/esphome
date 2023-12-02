#ifdef USE_ESP32
#include "esp32_hall.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include <driver/adc.h>

namespace esphome {
namespace esp32_hall {

static const char *const TAG = "esp32_hall";

void ESP32HallSensor::update() {
  adc1_config_width(ADC_WIDTH_BIT_12);
  int value_int = hall_sensor_read();
  float value = (value_int / 4095.0f) * 10000.0f;
  ESP_LOGD(TAG, "'%s': Got reading %.0f µT", this->name_.c_str(), value);
  this->publish_state(value);
}
std::string ESP32HallSensor::unique_id() { return get_mac_address() + "-hall"; }
void ESP32HallSensor::dump_config() { LOG_SENSOR("", "ESP32 Hall Sensor", this); }

}  // namespace esp32_hall
}  // namespace esphome

#endif
