#include "esphome/core/log.h"
#include "max17048.h"

namespace esphome::max17048 {
static const char *const TAG = "max17048.sensor";

constexpr uint16_t REG_VCELL = 0x02;  // ADC measurement of VCELL. LSB: 78.125µV/cell
constexpr uint16_t REG_SOC = 0x04;    // Battery state of charge.  LSB: 1%/256
constexpr uint16_t REG_CRATE = 0x16;  // Current rate. LSB: 0.208%/hr
constexpr uint16_t REG_MODE = 0x06;   // Operating mode

constexpr uint8_t REG_MODE_QS_CMD = 0x40;  // Flag to perform Quick-Start

constexpr float REG_CRATE_SCALE = 0.208f;                // 0.208%/hr
constexpr float REG_SOC_SCALE = 256.0f;                  // 1%/256
constexpr float REG_VCELL_SCALE = 78.125f / 1000000.0f;  // 78.125µV/cell

void MAX17048Component::setup() {
  ESP_LOGI(TAG, "Initialize MAX17048");
  this->initialize_sensor_();
}

void MAX17048Component::update() {
  uint16_t raw_reading;
  if (this->battery_voltage_sensor_ != nullptr) {
    if (!read_byte_16(REG_VCELL, &raw_reading)) {
      ESP_LOGW(TAG, "Unable to read VCELL register");
      return;
    }
    float voltage = static_cast<float>(raw_reading) * REG_VCELL_SCALE;
    this->battery_voltage_sensor_->publish_state(voltage);
  }

  if (this->battery_soc_sensor_ != nullptr) {
    if (!read_byte_16(REG_SOC, &raw_reading)) {
      ESP_LOGW(TAG, "Unable to read SOC register");
      return;
    }
    float soc_level = static_cast<float>(raw_reading) / REG_SOC_SCALE;
    this->battery_soc_sensor_->publish_state(soc_level);
  }

  if (this->battery_soc_rate_sensor_ != nullptr) {
    if (!read_byte_16(REG_CRATE, &raw_reading)) {
      ESP_LOGW(TAG, "Unable to read CRATE register");
      return;
    }
    float rate = static_cast<float>(static_cast<int16_t>(raw_reading)) * REG_CRATE_SCALE;
    this->battery_soc_rate_sensor_->publish_state(rate);
  }
}

void MAX17048Component::initialize_sensor_() {
  // Test communication and put sensor in defined state
  if (this->write_register16(REG_MODE, &REG_MODE_QS_CMD, 1) != i2c::NO_ERROR) {
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }
}

void MAX17048Component::dump_config() {
  ESP_LOGCONFIG(TAG, "MAX17048 Sensor:");
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);

  if (this->battery_voltage_sensor_ != nullptr) {
    LOG_SENSOR("  ", "Battery Voltage", this->battery_voltage_sensor_);
  }
  if (this->battery_soc_sensor_ != nullptr) {
    LOG_SENSOR("  ", "Battery Level", this->battery_soc_sensor_);
  }
  if (this->battery_soc_rate_sensor_ != nullptr) {
    LOG_SENSOR("  ", "Battery Rate", this->battery_soc_rate_sensor_);
  }

  if (this->is_failed()) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
  }
}
}  // namespace esphome::max17048
