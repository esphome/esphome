#include "esphome/core/log.h"
#include "stts22h.h"

namespace esphome {
namespace stts22h {

static const char *const TAG = "stts22h.sensor";

// I2C Registers
static const uint8_t WHOAMI_REG = 0x01;
static const uint8_t CTRL_REG = 0x04;
static const uint8_t TEMPERATURE_REG = 0x06;

static const uint8_t DEFAULT_CTRL_PAYLOAD = 0x89;  // Default control payload value
static const uint8_t WHOAMI_RESPONSE = 0xA0;       // Expected value for WHOAMI register

static const float SENSOR_SCALE = 0.01f;  // Sensor resolution in degrees Celsius

void STTS22H::setup() {
  // Initialize communication with sensor
  uint8_t whoami_value[1];
  if (this->read_register(WHOAMI_REG, whoami_value, 1) != i2c::NO_ERROR) {
    this->mark_failed(ESP_LOG_MSG_COMM_FAIL);
    return;
  }

  // Check if device is a STTS22H
  if (whoami_value[0] != WHOAMI_RESPONSE) {
    // If unexpected value received, log a warning, but do not fail the component
    ESP_LOGE(TAG, "Unexpected WHOAMI identifier received. Received: 0x%02X Expected: 0x%02X", whoami_value[0],
             WHOAMI_RESPONSE);
    this->mark_failed();
    return;
  }

  // Initialize sensor and put the sensor in a defined state
  if (this->configure_sensor() != i2c::NO_ERROR) {
    this->mark_failed("Failed to configure STTS22H sensor");
    return;
  }
}

void STTS22H::update() {
  float temperature = this->read_temperature();

  if (std::isnan(temperature)) {
    ESP_LOGW(TAG, "Temperature is NaN");
    return;
  }

  this->publish_state(temperature);
}

void STTS22H::dump_config() {
  LOG_SENSOR("", "STTS22H", this);
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
}

i2c::ErrorCode STTS22H::configure_sensor() {
  i2c::ErrorCode result = this->write_register(CTRL_REG, &DEFAULT_CTRL_PAYLOAD, 1);
  return result;
}

float STTS22H::read_temperature() {
  uint8_t temperature_register_value[2];
  if (this->read_register(TEMPERATURE_REG, temperature_register_value, 2) != i2c::NO_ERROR) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
    return NAN;  // Return NaN on communication failure
  }

  ESP_LOGW(TAG, "Low Byte: 0x%04X", temperature_register_value[0]);
  ESP_LOGW(TAG, "High Byte: 0x%04X", temperature_register_value[1]);

  // Combined raw value, unsigned and unaligned 16 bit
  int16_t temperature_raw = (static_cast<int16_t>(temperature_register_value[1]) << 8) | temperature_register_value[0];

  float temperature_value = temperature_raw * SENSOR_SCALE;  // Apply sensor resolution

  return temperature_value;
}

}  // namespace stts22h
}  // namespace esphome
