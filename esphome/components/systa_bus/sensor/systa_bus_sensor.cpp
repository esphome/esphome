#include "systa_bus_sensor.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::systa_bus {

static const char *const TAG = "systa_bus.sensor";

static inline int16_t get_i16be(std::vector<uint8_t> &message, uint16_t start) {
  return (int16_t) ((message[start] << 8) + message[start + 1]);
}

void SystaSolarAquaSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "SystaSolar Aqua:");
  LOG_SENSOR("  ", "Temperature TSA", this->temperature_tsa_sensor_);
  LOG_SENSOR("  ", "Temperature TSE", this->temperature_tse_sensor_);
  LOG_SENSOR("  ", "Temperature TWU", this->temperature_twu_sensor_);
  LOG_SENSOR("  ", "Temperature TW2", this->temperature_tw2_sensor_);
  LOG_SENSOR("  ", "Pump Speed", this->pump_speed_sensor_);
}

void SystaSolarAquaSensor::handle_message(std::vector<uint8_t> &message) {
  if (get_message_type(message) == 0xfc16) {
    if (this->temperature_tsa_sensor_ != nullptr)
      this->temperature_tsa_sensor_->publish_state(get_i16be(message, 4) * 0.1f);
    if (this->temperature_tse_sensor_ != nullptr)
      this->temperature_tse_sensor_->publish_state(get_i16be(message, 6) * 0.1f);
    if (this->temperature_twu_sensor_ != nullptr)
      this->temperature_twu_sensor_->publish_state(get_i16be(message, 8) * 0.1f);
    if (this->temperature_tw2_sensor_ != nullptr)
      this->temperature_tw2_sensor_->publish_state(get_i16be(message, 10) * 0.1f);
    if (this->pump_speed_sensor_ != nullptr)
      this->pump_speed_sensor_->publish_state(message[12]);
  }
}

}  // namespace esphome::systa_bus
