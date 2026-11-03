#include "xgzp68xx.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"
#include "esphome/components/i2c/i2c.h"

#include <cinttypes>

namespace esphome::xgzp68xx {

ESPHOME_LOG_TAG(TAG, "xgzp68xx.sensor");

static constexpr uint32_t MEASUREMENT_TIMEOUT_ID = 0;

static const uint8_t CMD_ADDRESS = 0x30;
static const uint8_t SYSCONFIG_ADDRESS = 0xA5;
static const uint8_t PCONFIG_ADDRESS = 0xA6;
static const uint8_t READ_COMMAND = 0x0A;

// Indexed by the XGZP68XXOversampling register value, which is not in size order
PROGMEM_STRING_TABLE(OversamplingStrings, "1024x", "2048x", "4096x", "8192x", "256x", "512x", "16384x", "32768x",
                     "UNKNOWN");

[[maybe_unused]] static const LogString *oversampling_to_str(XGZP68XXOversampling value) {
  return OversamplingStrings::get_log_str(static_cast<uint8_t>(value), OversamplingStrings::LAST_INDEX);
}

void XGZP68XXComponent::update() {
  // Do we need to change oversampling?
  if (this->last_pressure_oversampling_ != this->pressure_oversampling_) {
    uint8_t oldconfig = 0;
    this->read_register(PCONFIG_ADDRESS, &oldconfig, 1);
    uint8_t newconfig = (oldconfig & 0xf8) | (this->pressure_oversampling_ & 0x7);
    this->write_register(PCONFIG_ADDRESS, &newconfig, 1);
    ESP_LOGD(TAG, "oversampling to %s: oldconfig = 0x%x newconfig = 0x%x",
             LOG_STR_ARG(oversampling_to_str(this->pressure_oversampling_)), oldconfig, newconfig);
    this->last_pressure_oversampling_ = this->pressure_oversampling_;
  }

  // Request temp + pressure acquisition
  this->write_register(0x30, &READ_COMMAND, 1);

  // Wait 20mS per datasheet
  this->set_timeout(MEASUREMENT_TIMEOUT_ID, 20, [this]() {
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
    ESP_LOGV(TAG, "Got raw pressure=%" PRIu32 ", raw temperature=%u, K value=%u", pressure_raw, temperature_raw,
             this->k_value_);

    const float pressure_in_pa = static_cast<float>(sign_extend<24>(pressure_raw)) / this->k_value_;

    float temperature = ((float) (int16_t) temperature_raw) / 256.0f;

    PUBLISH_STATE(this->pressure_sensor_, pressure_in_pa);

    PUBLISH_STATE(this->temperature_sensor_, temperature);
  });  // end of set_timeout
}

void XGZP68XXComponent::setup() {
  uint8_t config1 = 0, config2 = 0;

  // Display some sample bits to confirm we are talking to the sensor
  if (i2c::ErrorCode::ERROR_OK != this->read_register(SYSCONFIG_ADDRESS, &config1, 1)) {
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }
  if (i2c::ErrorCode::ERROR_OK != this->read_register(PCONFIG_ADDRESS, &config2, 1)) {
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }
  ESP_LOGD(TAG, "sys_config 0x%x, p_config 0x%x", config1, config2);
}

void XGZP68XXComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "XGZP68xx:");
  LOG_SENSOR("  ", "Temperature: ", this->temperature_sensor_);
  LOG_SENSOR("  ", "Pressure: ", this->pressure_sensor_);
  if (this->pressure_sensor_ != nullptr) {
    ESP_LOGCONFIG(TAG, "    Oversampling: %s", LOG_STR_ARG(oversampling_to_str(this->pressure_oversampling_)));
  }
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
}

}  // namespace esphome::xgzp68xx
