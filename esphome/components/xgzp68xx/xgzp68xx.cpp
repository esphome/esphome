#include "xgzp68xx.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/components/i2c/i2c.h"

#include <cinttypes>

namespace esphome {
namespace xgzp68xx {

static const char *const TAG = "xgzp68xx.sensor";

static const uint8_t CMD_ADDRESS = 0x30;
static const uint8_t SYSCONFIG_ADDRESS = 0xA5;
static const uint8_t PCONFIG_ADDRESS = 0xA6;
static const uint8_t READ_COMMAND = 0x0A;

[[maybe_unused]] static const char *oversampling_to_str(XGZP68XXOversampling oversampling) {
  switch (oversampling) {
    case XGZP68XX_OVERSAMPLING_256X:
      return "256x";
    case XGZP68XX_OVERSAMPLING_512X:
      return "512x";
    case XGZP68XX_OVERSAMPLING_1024X:
      return "1024x";
    case XGZP68XX_OVERSAMPLING_2048X:
      return "2048x";
    case XGZP68XX_OVERSAMPLING_4096X:
      return "4096x";
    case XGZP68XX_OVERSAMPLING_8192X:
      return "8192x";
    case XGZP68XX_OVERSAMPLING_16384X:
      return "16384x";
    case XGZP68XX_OVERSAMPLING_32768X:
      return "32768x";
    default:
      return "UNKNOWN";
  }
}

void XGZP68XXComponent::update() {
  // Do we need to change oversampling?
  if (this->last_pressure_oversampling_ != this->pressure_oversampling_) {
    uint8_t oldconfig = 0;
    this->read_register(PCONFIG_ADDRESS, &oldconfig, 1);
    uint8_t newconfig = (oldconfig & 0xf8) | (this->pressure_oversampling_ & 0x7);
    this->write_register(PCONFIG_ADDRESS, &newconfig, 1);
    ESP_LOGD(TAG, "oversampling to %s: oldconfig = 0x%x newconfig = 0x%x",
             oversampling_to_str(this->pressure_oversampling_), oldconfig, newconfig);
    this->last_pressure_oversampling_ = this->pressure_oversampling_;
  }

  // Request temp + pressure acquisition
  this->write_register(0x30, &READ_COMMAND, 1);

  // Wait 20mS per datasheet
  this->set_timeout("measurement", 20, [this]() {
    uint8_t data[5] = {};
    uint32_t pressure_raw = 0;
    uint16_t temperature_raw = 0;
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
    ESP_LOGV(TAG, "Got raw pressure=%" PRIu32 ", raw temperature=%u", pressure_raw, temperature_raw);
    ESP_LOGV(TAG, "K value is %u", this->k_value_);

    // Sign extend the pressure
    float pressure_in_pa = (float) (((int32_t) pressure_raw << 8) >> 8);
    pressure_in_pa /= (float) (this->k_value_);

    float temperature = ((float) (int16_t) temperature_raw) / 256.0f;

    if (this->pressure_sensor_ != nullptr)
      this->pressure_sensor_->publish_state(pressure_in_pa);

    if (this->temperature_sensor_ != nullptr)
      this->temperature_sensor_->publish_state(temperature);
  });  // end of set_timeout
}

void XGZP68XXComponent::setup() {
  uint8_t config1 = 0, config2 = 0;

  // Display some sample bits to confirm we are talking to the sensor
  if (i2c::ErrorCode::ERROR_OK != this->read_register(SYSCONFIG_ADDRESS, &config1, 1)) {
    this->mark_failed();
    return;
  }
  if (i2c::ErrorCode::ERROR_OK != this->read_register(PCONFIG_ADDRESS, &config2, 1)) {
    this->mark_failed();
    return;
  }
  ESP_LOGD(TAG, "sys_config 0x%x, p_config 0x%x", config1, config2);
}

void XGZP68XXComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "XGZP68xx:");
  LOG_SENSOR("  ", "Temperature: ", this->temperature_sensor_);
  LOG_SENSOR("  ", "Pressure: ", this->pressure_sensor_);
  if (this->pressure_sensor_ != nullptr) {
    ESP_LOGCONFIG(TAG, "    Oversampling: %s", oversampling_to_str(this->pressure_oversampling_));
  }
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "  Connection failed");
  }
  LOG_UPDATE_INTERVAL(this);
}

}  // namespace xgzp68xx
}  // namespace esphome
