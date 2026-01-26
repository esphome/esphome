#include "esphome/core/log.h"
#include "max17048.h"

namespace esphome::max17048 {
static const char *const TAG = "max17048";

constexpr uint16_t REG_VCELL = 0x02;      // ADC measurement of VCELL. LSB: 78.125µV/cell
constexpr uint16_t REG_SOC = 0x04;        // Battery state of charge.  LSB: 1%/256
constexpr uint16_t REG_MODE = 0x06;       // Operating mode
constexpr uint16_t REG_VERSION = 0x08;    // Version number
constexpr uint16_t REG_HIBRT = 0x0B;      // Hibernate threshold. Read/write.     Default: 0x971C
constexpr uint16_t REG_CONFIG = 0x0C;     // Configuration register. Read/write. Default: 0x00FF
constexpr uint16_t REG_VALRT = 0x14;      // Voltage alert threshold. Read/write. Default: 0x00FF
constexpr uint16_t REG_CRATE = 0x16;      // Current rate. LSB: 0.208%/hr
constexpr uint16_t REG_VRESET_ID = 0x18;  //  VCELL-reset-threshold and chip ID. Read/write. Default: 0x96??
constexpr uint16_t REG_STATUS = 0x1A;     // Indicates overvoltage, undervoltage, SOC change, SOC low, and reset alerts.
// constexpr uint16_t REG_TABLE = 0x40 to 0x7F; // Configures battery parameters. Write-only.
constexpr uint16_t REG_CMD = 0xFE;  // Sends Power-On Reset (POR) command

void MAX17048Component::setup() { ESP_LOGCONFIG(TAG, "Setting up max17048"); }

void MAX17048Component::dump_config() {
  ESP_LOGCONFIG(TAG, "MAX17048:");
  LOG_I2C_DEVICE(this);
  LOG_SENSOR("  ", "Voltage (V)", this->battery_v_sensor_);
  LOG_SENSOR("  ", "State of Charge (%)", this->battery_soc_sensor_);
  LOG_SENSOR("  ", "Discharge Rate (%/hr)", this->battery_soc_rate_sensor_);
}

void MAX17048Component::update() {
  uint16_t raw_reading;
  if (this->battery_v_sensor_ != nullptr) {
    if (!read_byte_16(REG_VCELL, &raw_reading)) {
      ESP_LOGW(TAG, "'%s' - unable to read voltage register", this->name_.c_str());
      return;
    }
    float voltage = (float) raw_reading * 78.125 / 1000000;  // VCELL lsb is 78.125µV/cell per data sheet
    this->battery_v_sensor_->publish_state(voltage);
  }

  if (this->battery_soc_sensor_ != nullptr) {
    if (!read_byte_16(REG_SOC, &raw_reading)) {
      ESP_LOGW(TAG, "'%s' - unable to read percentage register", this->name_.c_str());
      return;
    }
    float percentage = (float) raw_reading / 256;  // SoC lsb is 1/256% per data sheet
    this->battery_soc_sensor_->publish_state(percentage);
  }

  if (this->battery_soc_rate_sensor_ != nullptr) {
    if (!read_byte_16(REG_CRATE, &raw_reading)) {
      ESP_LOGW(TAG, "'%s' - unable to read soc_rate register", this->name_.c_str());
      return;
    }
    float soc_rate = (float) (int16_t) raw_reading * 0.208;  // Current rate lsb is 0.208%/hr per data sheet
    this->battery_soc_rate_sensor_->publish_state(soc_rate);
  }
}
}  // namespace esphome::max17048
