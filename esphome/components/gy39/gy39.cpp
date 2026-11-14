#include "gy39.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::gy39 {

ESPHOME_LOG_TAG(TAG, "gy39");

// Registers 0x00-0x0D, big-endian: lux x100 (u32), °C x100 (s16), Pa x100 (u32), %RH x100 (u16), metres (s16)
static constexpr uint8_t REGISTER_DATA = 0x00;
static constexpr size_t DATA_SIZE = 14;

void GY39::setup() {
  uint8_t data[DATA_SIZE];
  this->setup_check(this->read_bytes(REGISTER_DATA, data, sizeof(data)));
}

void GY39::update() {
  uint8_t data[DATA_SIZE];
  if (!this->status_comm_result(this->read_bytes(REGISTER_DATA, data, sizeof(data))))
    return;
  PUBLISH_STATE(this->illuminance_sensor_, encode_value<uint32_t>(data) / 100.0f);
  PUBLISH_STATE(this->temperature_sensor_, encode_value<int16_t>(data + 4) / 100.0f);
  PUBLISH_STATE(this->pressure_sensor_, encode_value<uint32_t>(data + 6) / 100.0f);
  PUBLISH_STATE(this->humidity_sensor_, encode_value<uint16_t>(data + 10) / 100.0f);
  PUBLISH_STATE(this->altitude_sensor_, encode_value<int16_t>(data + 12));
}

void GY39::dump_config() {
  ESP_LOGCONFIG(TAG, "GY-39:");
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Illuminance", this->illuminance_sensor_);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Pressure", this->pressure_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
  LOG_SENSOR("  ", "Altitude", this->altitude_sensor_);
}

}  // namespace esphome::gy39
