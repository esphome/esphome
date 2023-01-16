#include "grove_gas_mc_v2.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

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

#define READ_SENSOR(read_var, read_cmd) \
  if (!this->read_bytes((read_cmd), (uint8_t *) &(read_var), 4)) { \
    ESP_LOGW(TAG, "Reading Grove Gas Sensor data failed!"); \
    this->error_code_ = COMMUNICATION_FAILED; \
    this->status_set_warning(); \
    return; \
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
  uint32_t no2, ethanol, voc, co;

  // Read from each of the gas sensors
  READ_SENSOR(no2, GROVE_GAS_MC_V2_READ_GM102B)
  READ_SENSOR(ethanol, GROVE_GAS_MC_V2_READ_GM302B)
  READ_SENSOR(voc, GROVE_GAS_MC_V2_READ_GM502B)
  READ_SENSOR(co, GROVE_GAS_MC_V2_READ_GM702B)

  ESP_LOGD(TAG, "Got no2=%u, c2h5oh=%u, voc=%u, co=%u", no2, ethanol, voc, co);

  if (this->no2_ != nullptr) {
    this->no2_->publish_state(no2);
  }

  if (this->ethanol_ != nullptr) {
    this->ethanol_->publish_state(ethanol);
  }

  if (this->tvoc_ != nullptr) {
    this->tvoc_->publish_state(voc);
  }

  if (this->co_ != nullptr) {
    this->co_->publish_state(co);
  }

  this->status_clear_warning();
}

void GroveGasMultichannelV2Component::dump_config() {
  ESP_LOGCONFIG(TAG, "Grove Multichannel Gas Sensor V2");
  LOG_I2C_DEVICE(this)
  LOG_UPDATE_INTERVAL(this)
  LOG_SENSOR("  ", "NO2 Sensor", this->no2_)
  LOG_SENSOR("  ", "Ethanol Sensor", this->ethanol_)
  LOG_SENSOR("  ", "CO2 Sensor", this->co_)
  LOG_SENSOR("  ", "TVOC Sensor", this->tvoc_)

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
