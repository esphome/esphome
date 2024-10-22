#include "max17043.h"
#include "esphome/core/log.h"

namespace esphome {
namespace max17043 {

// MAX174043 is a 1-Cell Fuel Gauge with ModelGauge and Low-Battery Alert
// Consult the datasheet at https://www.analog.com/en/products/max17043.html

static const char *const TAG = "max17043";

static const uint8_t MAX17043_VCELL = 0x02;
static const uint8_t MAX17043_SOC = 0x04;
static const uint8_t MAX17043_CONFIG = 0x0c;

static const uint16_t MAX17043_CONFIG_POWER_UP_DEFAULT = 0x971C;
static const uint16_t MAX17043_CONFIG_SAFE_MASK = 0xFF1F;  // mask out sleep bit (7), unused bit (6) and alert bit (4)
static const uint16_t MAX17043_CONFIG_SLEEP_MASK = 0x0080;

void MAX17043Component::update() {
  uint16_t raw_voltage, raw_percent;

  if (this->voltage_sensor_ != nullptr) {
    if (!this->read_byte_16(MAX17043_VCELL, &raw_voltage)) {
      this->status_set_warning("Unable to read MAX17043_VCELL");
    } else {
      float voltage = (1.25 * (float) (raw_voltage >> 4)) / 1000.0;
      this->voltage_sensor_->publish_state(voltage);
      this->status_clear_warning();
    }
  }
  if (this->battery_remaining_sensor_ != nullptr) {
    if (!this->read_byte_16(MAX17043_SOC, &raw_percent)) {
      this->status_set_warning("Unable to read MAX17043_SOC");
    } else {
      float percent = (float) ((raw_percent >> 8) + 0.003906f * (raw_percent & 0x00ff));
      this->battery_remaining_sensor_->publish_state(percent);
      this->status_clear_warning();
    }
  }
}

void MAX17043Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up MAX17043...");

  uint16_t config_reg;
  if (this->write(&MAX17043_CONFIG, 1) != i2c::ERROR_OK) {
    this->status_set_warning();
    return;
  }

  if (this->read(reinterpret_cast<uint8_t *>(&config_reg), 2) != i2c::ERROR_OK) {
    this->status_set_warning();
    return;
  }

  config_reg = i2c::i2ctohs(config_reg) & MAX17043_CONFIG_SAFE_MASK;
  ESP_LOGV(TAG, "MAX17043 CONFIG register reads 0x%X", config_reg);

  if (config_reg != MAX17043_CONFIG_POWER_UP_DEFAULT) {
    ESP_LOGE(TAG, "Device does not appear to be a MAX17043");
    this->status_set_error("unrecognised");
    this->mark_failed();
    return;
  }

  // need to write back to config register to reset the sleep bit
  if (!this->write_byte_16(MAX17043_CONFIG, MAX17043_CONFIG_POWER_UP_DEFAULT)) {
    this->status_set_error("sleep reset failed");
    this->mark_failed();
    return;
  }
}

void MAX17043Component::dump_config() {
  ESP_LOGCONFIG(TAG, "MAX17043:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with MAX17043 failed");
  }
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Battery Voltage", this->voltage_sensor_);
  LOG_SENSOR("  ", "Battery Level", this->battery_remaining_sensor_);
}

float MAX17043Component::get_setup_priority() const { return setup_priority::DATA; }

void MAX17043Component::sleep_mode() {
  if (!this->is_failed()) {
    if (!this->write_byte_16(MAX17043_CONFIG, MAX17043_CONFIG_POWER_UP_DEFAULT | MAX17043_CONFIG_SLEEP_MASK)) {
      ESP_LOGW(TAG, "Unable to write the sleep bit to config register");
      this->status_set_warning();
    }
  }
}

}  // namespace max17043
}  // namespace esphome
