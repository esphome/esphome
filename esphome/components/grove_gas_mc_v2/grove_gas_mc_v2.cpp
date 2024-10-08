#include "grove_gas_mc_v2.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace grove_gas_mc_v2 {

static const char *const TAG = "grove_gas_mc_v2";

// I2C Commands for Grove Gas Multichannel V2 Sensor
// Taken from:
// https://github.com/Seeed-Studio/Seeed_Arduino_MultiGas/blob/master/src/Multichannel_Gas_GroveGasMultichannelV2.h
static const uint8_t GROVE_GAS_MC_V2_HEAT_ON = 0xFE;
static const uint8_t GROVE_GAS_MC_V2_HEAT_OFF = 0xFF;
static const uint8_t GROVE_GAS_MC_V2_READ_GM102B = 0x01;
static const uint8_t GROVE_GAS_MC_V2_READ_GM302B = 0x03;
static const uint8_t GROVE_GAS_MC_V2_READ_GM502B = 0x05;
static const uint8_t GROVE_GAS_MC_V2_READ_GM702B = 0x07;

bool GroveGasMultichannelV2Component::read_sensor_(uint8_t address, sensor::Sensor *sensor) {
  if (sensor == nullptr) {
    return true;
  }
  uint32_t value = 0;
  if (!this->read_bytes(address, (uint8_t *) &value, 4)) {
    ESP_LOGW(TAG, "Reading Grove Gas Sensor data failed!");
    this->error_code_ = COMMUNICATION_FAILED;
    this->status_set_warning();
    return false;
  }
  sensor->publish_state(value);
  return true;
}

void GroveGasMultichannelV2Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Grove Multichannel Gas Sensor V2...");

  // Before reading sensor values, must preheat sensor
  if (!(this->write_bytes(GROVE_GAS_MC_V2_HEAT_ON, {}))) {
    this->mark_failed();
    this->error_code_ = APP_START_FAILED;
  }
}

void GroveGasMultichannelV2Component::update() {
  // Read from each of the gas sensors
  if (!this->read_sensor_(GROVE_GAS_MC_V2_READ_GM102B, this->nitrogen_dioxide_sensor_))
    return;
  if (!this->read_sensor_(GROVE_GAS_MC_V2_READ_GM302B, this->ethanol_sensor_))
    return;
  if (!this->read_sensor_(GROVE_GAS_MC_V2_READ_GM502B, this->tvoc_sensor_))
    return;
  if (!this->read_sensor_(GROVE_GAS_MC_V2_READ_GM702B, this->carbon_monoxide_sensor_))
    return;

  this->status_clear_warning();
}

void GroveGasMultichannelV2Component::dump_config() {
  ESP_LOGCONFIG(TAG, "Grove Multichannel Gas Sensor V2");
  LOG_I2C_DEVICE(this)
  LOG_UPDATE_INTERVAL(this)
  LOG_SENSOR("  ", "Nitrogen Dioxide", this->nitrogen_dioxide_sensor_)
  LOG_SENSOR("  ", "Ethanol", this->ethanol_sensor_)
  LOG_SENSOR("  ", "Carbon Monoxide", this->carbon_monoxide_sensor_)
  LOG_SENSOR("  ", "TVOC", this->tvoc_sensor_)

  if (this->is_failed()) {
    switch (this->error_code_) {
      case COMMUNICATION_FAILED:
        ESP_LOGW(TAG, "Communication failed! Is the sensor connected?");
        break;
      case APP_INVALID:
        ESP_LOGW(TAG, "Sensor reported invalid APP installed.");
        break;
      case APP_START_FAILED:
        ESP_LOGW(TAG, "Sensor reported APP start failed.");
        break;
      case UNKNOWN:
      default:
        ESP_LOGW(TAG, "Unknown setup error!");
        break;
    }
  }
}

}  // namespace grove_gas_mc_v2
}  // namespace esphome
