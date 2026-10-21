#include "systa_bus_sensor.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::systa_bus {

static const char *const TAG = "systa_bus.sensor";

static int16_t get_i16be(std::span<const uint8_t> message, size_t start) {
  return static_cast<int16_t>(encode_uint16(message[start], message[start + 1]));
}

void SystaSolarAquaSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "SystaSolar Aqua:");
  LOG_SENSOR("  ", "Temperature TSA", this->temperature_tsa_sensor_);
  LOG_SENSOR("  ", "Temperature TSE", this->temperature_tse_sensor_);
  LOG_SENSOR("  ", "Temperature TWU", this->temperature_twu_sensor_);
  LOG_SENSOR("  ", "Temperature TW2", this->temperature_tw2_sensor_);
  LOG_SENSOR("  ", "Pump Speed", this->pump_speed_sensor_);
}

void SystaSolarAquaSensor::handle_message(std::span<const uint8_t> message) {
  if (get_message_type(message) != MESSAGE_TYPE_AQUA_SENSOR_DATA)
    return;
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

}  // namespace esphome::systa_bus
