#include "xgzp68xx.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace xgzp68xx {

static const char *const TAG = "xgzp68xx.sensor";

static const uint8_t CMD_ADDRESS = 0x30;
static const uint8_t SYSCONFIG_ADDRESS = 0xA5;
static const uint8_t PCONFIG_ADDRESS = 0xA6;
static const uint8_t READ_COMMAND = 0x0A;

void XGZP68XXComponent::update() { 
  uint8_t data[5];

  uint32_t pressure_raw;
  uint16_t temperature_raw;

  // Request temp + pressure acquisition
  this->write_register(0x30, &READ_COMMAND, 1);
  
  // Wait 20mS per datasheet
  delay(20);

  // Read the sensor data
  this->read_register(0x06, data, 5);
  pressure_raw = encode_uint24(data[0], data[1], data[2]);
  temperature_raw = encode_uint16(data[3], data[4]);
  
  // Convert the pressure data to hPa
  ESP_LOGV(TAG, "Got raw pressure=%d, raw temperature=%d ", pressure_raw, temperature_raw);

  float pressure_in_pa;
  if (pressure_raw > 8388608) {
    // Positive pressure
    pressure_in_pa = (pressure_raw - 16777216) / 4096.0f;
  } else {
    // Negative pressure
    pressure_in_pa = pressure_raw / 4096.0f;
  }


  uint16_t temp;
  if (temperature_raw < pow(2, 15)) {
      temp = (float)temperature_raw / 256.0;
  } else {
      temp = (float)(temperature_raw - pow(2, 16)) / 256.0;
  }

  if (this->pressure_sensor_ != nullptr)
    this->pressure_sensor_->publish_state(pressure_in_pa);
  
  if (this->temperature_sensor_ != nullptr)
    this->temperature_sensor_->publish_state(temp);
  

}

void XGZP68XXComponent::setup() {
  ESP_LOGD(TAG, "Setting up XGZP68xx...");
  uint8_t config;

  // Display some sample bits to confirm we are talking to the sensor
  this->read_register(SYSCONFIG_ADDRESS, &config, 1);
  ESP_LOGCONFIG(TAG, "Gain bits are %03b", (config >> 3) & 0b111);
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
