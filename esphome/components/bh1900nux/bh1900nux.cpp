#include "esphome/core/log.h"
#include "bh1900nux.h"

namespace esphome {
namespace bh1900nux {

static const char *const TAG = "bh1900nux.sensor";

// I2C Registers
static const uint8_t TEMPERATURE_REG = 0x00;
static const uint8_t CONFIG_REG = 0x01;            // Not used and supported yet
static const uint8_t TEMPERATURE_LOW_REG = 0x02;   // Not used and supported yet
static const uint8_t TEMPERATURE_HIGH_REG = 0x03;  // Not used and supported yet
static const uint8_t SOFT_RESET_REG = 0x04;

// I2C Command payloads
static const uint8_t SOFT_RESET_PAYLOAD = 0x01;  // Soft Reset value

static const float SENSOR_RESOLUTION = 0.0625f;  // Sensor resolution per bit in degrees celsius

void BH1900NUXSensor::setup() {
  // Initialize I2C device
  i2c::ErrorCode result_code =
      this->write_register(SOFT_RESET_REG, &SOFT_RESET_PAYLOAD, 1);  // Software Reset to check communication
  if (result_code != i2c::ERROR_OK) {
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }
}

void BH1900NUXSensor::update() {
  uint8_t temperature_raw[2];
  if (this->read_register(TEMPERATURE_REG, temperature_raw, 2) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
    return;
  }

  // Combined raw value, unsigned and unaligned 16 bit
  // Temperature is represented in just 12 bits, shift needed
  int16_t raw_temperature_register_value = encode_uint16(temperature_raw[0], temperature_raw[1]);
  raw_temperature_register_value >>= 4;
  float temperature_value = raw_temperature_register_value * SENSOR_RESOLUTION;  // Apply sensor resolution

  this->publish_state(temperature_value);
}

void BH1900NUXSensor::dump_config() {
  LOG_SENSOR("", "BH1900NUX", this);
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
}

}  // namespace bh1900nux
}  // namespace esphome
