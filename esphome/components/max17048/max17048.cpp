#include "max17048.h"
#include "esphome/core/log.h"

namespace esphome::max17048 {

// MAX17048/MAX17049 1 and 2 cell fuel gauges with ModelGauge
// Datasheet: https://www.analog.com/media/en/technical-documentation/data-sheets/MAX17048-MAX17049.pdf

ESPHOME_LOG_TAG(TAG, "max17048");

static constexpr uint8_t REG_VCELL = 0x02;    // cell voltage
static constexpr uint8_t REG_SOC = 0x04;      // state of charge
static constexpr uint8_t REG_VERSION = 0x08;  // 0x001x
static constexpr uint8_t REG_CRATE = 0x16;    // charge rate, signed

static constexpr uint16_t VERSION_MASK = 0xFFF0;
static constexpr uint16_t VERSION_FAMILY = 0x0010;

static constexpr float VCELL_VOLTS_PER_LSB = 78.125e-6f;
static constexpr float SOC_PERCENT_PER_LSB = 1.0f / 256.0f;
static constexpr float CRATE_PERCENT_PER_HOUR_PER_LSB = 0.208f;

void MAX17048Component::setup() {
  uint16_t version;
  if (!this->read_byte_16(REG_VERSION, &version)) {
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }
  if ((version & VERSION_MASK) != VERSION_FAMILY) {
    ESP_LOGW(TAG, "Unexpected version 0x%04X", version);
  }
}

bool MAX17048Component::read_register_(uint8_t reg, uint16_t &value) {
  if (!this->read_byte_16(reg, &value)) {
    this->status_set_warning(LOG_STR("Register read failed"));
    return false;
  }
  this->status_clear_warning();
  return true;
}

void MAX17048Component::update() {
  uint16_t raw;
  if (this->battery_voltage_sensor_ != nullptr && this->read_register_(REG_VCELL, raw)) {
    this->battery_voltage_sensor_->publish_state(raw * VCELL_VOLTS_PER_LSB);
  }
  if (this->battery_level_sensor_ != nullptr && this->read_register_(REG_SOC, raw)) {
    this->battery_level_sensor_->publish_state(raw * SOC_PERCENT_PER_LSB);
  }
  if (this->rate_sensor_ != nullptr && this->read_register_(REG_CRATE, raw)) {
    this->rate_sensor_->publish_state(static_cast<int16_t>(raw) * CRATE_PERCENT_PER_HOUR_PER_LSB);
  }
}

void MAX17048Component::dump_config() {
  ESP_LOGCONFIG(TAG, "MAX17048:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
  }
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Battery Voltage", this->battery_voltage_sensor_);
  LOG_SENSOR("  ", "Battery Level", this->battery_level_sensor_);
  LOG_SENSOR("  ", "Rate", this->rate_sensor_);
}

}  // namespace esphome::max17048
