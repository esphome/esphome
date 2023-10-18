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

void XGZP68XXComponent::update() { 
  uint8_t press_msb, press_csb, press_lsb;
  uint8_t temp_msb, temp_lsb;

  // Request temp + pressure acquisition
  this->write_byte(0x30, 0x0A);

  // Wait 20mS per datasheet
  delay(20);

  // Read the sensor data
  this->read_byte(0x06, &press_msb);
  this->read_byte(0x07, &press_csb);
  this->read_byte(0x08, &press_lsb);
  this->read_byte(0x09, &temp_msb);
  this->read_byte(0x0A, &temp_lsb);

  // Convert the pressure data to hPa
  uint32_t pressure_raw = ((uint32_t)press_msb << 16) | ((uint32_t)press_csb << 8) | (uint32_t)press_lsb;
  uint32_t temperature_raw = ((uint32_t)temp_msb << 8) | (uint32_t)temp_lsb;
  
  ESP_LOGV(TAG, "Got raw pressure=%d, raw temperature=%d ", pressure_raw, temperature_raw);

  float pressure_in_pa;
  if (pressure_raw & 0x800000) {
    // Negative pressure
    pressure_in_pa = (pressure_raw - 0x1000000) / 4096.0f;
  } else {
    // Positive pressure
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
  this->read_byte(SYSCONFIG_ADDRESS, &config);
  ESP_LOGCONFIG(TAG, "Gain bits are %03b", (config >> 3) & 0b111);
  ESP_LOGCONFIG(TAG, "XGZP68xx started!");
}

void XGZP68XXComponent::dump_config() {
  LOG_SENSOR("  ", "XGZP68xx", this);
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "  Connection with XGZP68xx failed!");
  }
  LOG_UPDATE_INTERVAL(this);
}



}  // namespace xgzp68xx
}  // namespace esphome
