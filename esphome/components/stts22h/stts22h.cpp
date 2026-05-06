#include "esphome/core/log.h"
#include "stts22h.h"

namespace esphome::stts22h {

static const char *const TAG = "stts22h";

static const uint8_t WHOAMI_REG = 0x01;
static const uint8_t CTRL_REG = 0x04;
static const uint8_t TEMPERATURE_REG = 0x06;

// CTRL_REG flags
static const uint8_t LOW_ODR_CTRL_ENABLE_FLAG = 0x80;  // Flag to enable low ODR mode in CTRL_REG
static const uint8_t FREERUN_CTRL_ENABLE_FLAG = 0x04;  // Flag to enable FREERUN mode in CTRL_REG
static const uint8_t ADD_INC_ENABLE_FLAG = 0x08;       // Flag to enable ADD_INC (IF_ADD_INC) mode in CTRL_REG

static const uint8_t WHOAMI_STTS22H_IDENTIFICATION = 0xA0;  // ID value of STTS22H in WHOAMI_REG

static const float SENSOR_SCALE = 0.01f;  // Sensor resolution in degrees Celsius

void STTS22HComponent::setup() {
  // Check if device is a STTS22H
  if (!this->is_stts22h_sensor_()) {
    this->mark_failed(LOG_STR("Device is not a STTS22H sensor"));
    return;
  }

  this->initialize_sensor_();
}

void STTS22HComponent::update() {
  if (this->is_failed()) {
    return;
  }

  this->publish_state(this->read_temperature_());
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
  uint8_t temp_reg_value[2];
  if (this->read_register(TEMPERATURE_REG, temp_reg_value, 2) != i2c::NO_ERROR) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
    return NAN;
  }

  // Combine the two bytes into a single 16-bit signed integer
  // The STTS22H temperature data is in two's complement format
  int16_t temp_raw_value = static_cast<int16_t>(encode_uint16(temp_reg_value[1], temp_reg_value[0]));
  return temp_raw_value * SENSOR_SCALE;  // Apply sensor resolution
}

bool STTS22HComponent::is_stts22h_sensor_() {
  uint8_t whoami_value;
  if (this->read_register(WHOAMI_REG, &whoami_value, 1) != i2c::NO_ERROR) {
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return false;
  }

  if (whoami_value != WHOAMI_STTS22H_IDENTIFICATION) {
    this->mark_failed(LOG_STR("Unexpected WHOAMI identifier. Sensor is not a STTS22H"));
    return false;
  }

  return true;
}

void STTS22HComponent::initialize_sensor_() {
  // Read current CTRL_REG configuration
  uint8_t ctrl_value;
  if (this->read_register(CTRL_REG, &ctrl_value, 1) != i2c::NO_ERROR) {
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }

  // Enable low ODR mode and enable ADD_INC
  // Before low ODR mode can be used,
  // FREERUN bit must be cleared (see sensor documentation)
  ctrl_value &= ~FREERUN_CTRL_ENABLE_FLAG;  // Clear FREERUN bit
  if (this->write_register(CTRL_REG, &ctrl_value, 1) != i2c::NO_ERROR) {
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }

  // Enable LOW ODR mode and ADD_INC
  ctrl_value |= LOW_ODR_CTRL_ENABLE_FLAG | ADD_INC_ENABLE_FLAG;  // Set LOW ODR bit and ADD_INC bit
  if (this->write_register(CTRL_REG, &ctrl_value, 1) != i2c::NO_ERROR) {
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }
}

}  // namespace esphome::stts22h
