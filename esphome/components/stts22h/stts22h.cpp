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
  if (!this->is_stts22h_sensor_()) {
    this->mark_failed("Device is not a STTS22H sensor");
    return;
  }

  this->initialize_sensor_();
}

void STTS22HComponent::update() {
  if (this->is_failed()) {
    return;
  }

  float temperature = this->read_temperature_();
  if (std::isnan(temperature)) {
    ESP_LOGW(TAG, "Temperature is NaN");
    return;
  }

  ESP_LOGI(TAG, "Is published");
  this->publish_state(temperature);
}

void STTS22HComponent::dump_config() {
  LOG_SENSOR("", "STTS22H", this);
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
  }
}

float STTS22HComponent::read_temperature_() {
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

/// @brief Reads the harcoded ID whih identifies device on the bus as STTS22H sensor.
/// @return
bool STTS22HComponent::is_stts22h_sensor_() {
  uint8_t whoami_value[1];
  if (this->read_register(WHOAMI_REG, whoami_value, 1) != i2c::NO_ERROR) {
    this->mark_failed(ESP_LOG_MSG_COMM_FAIL);
    return false;
  }

  if (whoami_value[0] != WHOAMI_STTS22H_IDENTIFICATION) {
    this->mark_failed("Unexpected WHOAMI identifier. Sensor is not a STTS22H");
    return false;
  }

  return true;
}

void STTS22HComponent::initialize_sensor_() {
  uint8_t ctrl_value[1];
  if (this->read_register(CTRL_REG, ctrl_value, 1) != i2c::NO_ERROR) {
    this->mark_failed(ESP_LOG_MSG_COMM_FAIL);
    return;
  }

  // Enable low ODR mode and auto increment in CTRL_REG
  // Enable auto increment of register address in CTRL_REG
  ctrl_value[0] = ctrl_value[0] | AUTO_INC_CTRL_ENABLE_FLAG | LOW_ODR_CTRL_ENABLE_FLAG;
  if (this->write_register(CTRL_REG, ctrl_value, 1) != i2c::NO_ERROR) {
    this->mark_failed(ESP_LOG_MSG_COMM_FAIL);
    return;
  }
}

}  // namespace stts22h
}  // namespace esphome
