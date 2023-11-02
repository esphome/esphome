#include "xgzp68xx.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace xgzp68xx {

static const char *const TAG = "xgzp68xx.sensor";

static const uint8_t CMD_ADDRESS = 0x30;
static const uint8_t SYSCONFIG_ADDRESS = 0xA5;
static const uint8_t PCONFIG_ADDRESS = 0xA6;
static const uint8_t READ_COMMAND = 0x0A;

void XGZP68XXComponent::update() {
  // Request temp + pressure acquisition
  this->write_register(0x30, &READ_COMMAND, 1);

  // Wait 20mS per datasheet
  this->set_timeout("measurement", 20, [this]() {
    uint8_t data[5];
    uint32_t pressure_raw;
    uint16_t temperature_raw;
    float pressure_in_pa, temperature;
    int success;

    // Read the sensor data
    success = this->read_register(0x06, data, 5);
    if (success != 0) {
      ESP_LOGE(TAG, "Failed to read sensor data! Error code: %i", success);
      return;
    }

    pressure_raw = encode_uint24(data[0], data[1], data[2]);
    temperature_raw = encode_uint16(data[3], data[4]);

    // Convert the pressure data to hPa
    ESP_LOGV(TAG, "Got raw pressure=%d, raw temperature=%d ", pressure_raw, temperature_raw);
    ESP_LOGV(TAG, "K value is %d ", this->k_value_);

    // The most significant bit of both pressure and temperature will be 1 to indicate a negative value.
    // This is directly from the datasheet, and the calculations below will handle this.
    if (pressure_raw > pow(2, 23)) {
      // Negative pressure
      pressure_in_pa = (pressure_raw - pow(2, 24)) / (float) (this->k_value_);
    } else {
      // Positive pressure
      pressure_in_pa = pressure_raw / (float) (this->k_value_);
    }

    if (temperature_raw > pow(2, 15)) {
      // Negative temperature
      temperature = (float) (temperature_raw - pow(2, 16)) / 256.0f;
    } else {
      // Positive temperature
      temperature = (float) temperature_raw / 256.0f;
    }

    if (this->pressure_sensor_ != nullptr)
      this->pressure_sensor_->publish_state(pressure_in_pa);

    if (this->temperature_sensor_ != nullptr)
      this->temperature_sensor_->publish_state(temperature);
  });  // end of set_timeout
}

void XGZP68XXComponent::setup() {
  ESP_LOGD(TAG, "Setting up XGZP68xx...");
  uint8_t config;

  // Display some sample bits to confirm we are talking to the sensor
  this->read_register(SYSCONFIG_ADDRESS, &config, 1);
  ESP_LOGCONFIG(TAG, "Gain value is %d", (config >> 3) & 0b111);
  ESP_LOGCONFIG(TAG, "XGZP68xx started!");
}

void XGZP68XXComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "XGZP68xx");
  LOG_SENSOR("  ", "Temperature: ", this->temperature_sensor_);
  LOG_SENSOR("  ", "Pressure: ", this->pressure_sensor_);
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "  Connection with XGZP68xx failed!");
  }
  LOG_UPDATE_INTERVAL(this);
}

}  // namespace xgzp68xx
}  // namespace esphome
