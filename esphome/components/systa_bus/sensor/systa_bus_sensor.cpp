#include "systa_bus_sensor.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::systa_bus {

ESPHOME_LOG_TAG(TAG, "systa_bus.sensor");

static int16_t get_i16be(std::span<const uint8_t> message, size_t start) {
  return encode_value<int16_t>(&message[start]);
}

void SystaSolarAquaSensor::dump_child_config() {
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
  PUBLISH_STATE(this->temperature_tsa_sensor_, get_i16be(message, 4) * 0.1f);
  PUBLISH_STATE(this->temperature_tse_sensor_, get_i16be(message, 6) * 0.1f);
  PUBLISH_STATE(this->temperature_twu_sensor_, get_i16be(message, 8) * 0.1f);
  PUBLISH_STATE(this->temperature_tw2_sensor_, get_i16be(message, 10) * 0.1f);
  PUBLISH_STATE(this->pump_speed_sensor_, message[12]);
}

}  // namespace esphome::systa_bus
