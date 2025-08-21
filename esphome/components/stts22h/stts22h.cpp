#include "esphome/core/log.h"
#include "stts22h.h"

namespace esphome {
namespace stts22h {

static const char *const TAG = "stts22h";

static const uint8_t WHOAMI_REG = 0x01;
static const uint8_t CTRL_REG = 0x04;
static const uint8_t TEMPERATURE_REG = 0x06;

// CTRL_REG flags
static const uint8_t LOW_ODR_CTRL_ENABLE_FLAG = 0x80;   // Flag to enable low ODR mode in CTRL_REG
static const uint8_t AUTO_INC_CTRL_ENABLE_FLAG = 0x08;  // Flag to enable auto increment in CTRL_REG

static const uint8_t WHOAMI_STTS22H_IDENTIFICATION = 0xA0;  // ID value of STTS22H in WHOAMI_REG

static const float SENSOR_SCALE = 0.01f;  // Sensor resolution in degrees Celsius

void STTS22HComponent::setup() {
  // Check if device is a STTS22H
  uint8_t sensor_id = this->read_sensor_identification();
  if (sensor_id != WHOAMI_STTS22H_IDENTIFICATION) {
    this->mark_failed("Unexpected WHOAMI identifier. Sensor is not a STTS22H");
    return;
  }

  // Enable low ODR (Output Data Rate) operation mode
  // TODO: Implement one-shot mode
  // TODO: Implement freerun mode
  this->enable_low_odr_operation_mode();

  this->enable_reg_adr_auto_increment();
}

void STTS22HComponent::update() {
  if (this->is_failed()) {
    return;
  }

  float temperature = this->read_temperature();
  if (std::isnan(temperature)) {
    ESP_LOGW(TAG, "Temperature is NaN");
    return;
  }

  this->publish_state(temperature);
}

void STTS22HComponent::dump_config() {
  LOG_SENSOR("", "STTS22H", this);
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
}

float STTS22HComponent::read_temperature() {
  uint8_t temperature_register_value[2];
  if (this->read_register(TEMPERATURE_REG, temperature_register_value, 2) != i2c::NO_ERROR) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
    return NAN;
  }

  // Combine the two bytes into a single 16-bit signed integer
  // The STTS22H temperature data is in two's complement format
  int16_t temperature_raw = (static_cast<int16_t>(temperature_register_value[1]) << 8) | temperature_register_value[0];
  float temperature_value = temperature_raw * SENSOR_SCALE;  // Apply sensor resolution

  return temperature_value;
}

uint8_t STTS22HComponent::read_sensor_identification() {
  uint8_t whoami_value[1];
  if (this->read_register(WHOAMI_REG, whoami_value, 1) != i2c::NO_ERROR) {
    this->mark_failed(ESP_LOG_MSG_COMM_FAIL);
    return 0XFF;  // Return an invalid value to indicate failure
  }

  return whoami_value[0];
}

/// @brief Sets the ODR (Output Data Rate) to 1Hz
/// The sensor will measure the temperature at 1Hz
void STTS22HComponent::enable_low_odr_operation_mode() {
  if (this->write_register(CTRL_REG, &LOW_ODR_CTRL_ENABLE_FLAG, 1) != i2c::NO_ERROR) {
    this->mark_failed(ESP_LOG_MSG_COMM_FAIL);
    return;
  }
}

/// @brief Enable automatic address increment when multiple I²C read and write transactions are used at once.
/// when multiple I²C read and write transactions are used at once.
void STTS22HComponent::enable_reg_adr_auto_increment() {
  if (this->write_register(CTRL_REG, &AUTO_INC_CTRL_ENABLE_FLAG, 1) != i2c::NO_ERROR) {
    this->mark_failed(ESP_LOG_MSG_COMM_FAIL);
    return;
  }
}

}  // namespace stts22h
}  // namespace esphome
