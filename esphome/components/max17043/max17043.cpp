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

void MAX17043Component::update() {
  uint16_t raw_voltage, raw_percent;
  if (!this->read_data_(&raw_voltage, &raw_percent)) {
    this->status_set_warning();
    return;
  }

  float voltage = (1.25 * (float) (raw_voltage >> 4)) / 1000.0;

  if (this->voltage_sensor_ != nullptr)
    this->voltage_sensor_->publish_state(voltage);

  // During charging the percentage might be (slightly) above 100%. To avoid strange numbers
  // in the statistics the percentage provided by this driver will not go above 100%
  float percent = (float) ((raw_percent >> 8) + 0.003906f * (raw_percent & 0x00ff));
  if (percent > 100.0) {
    percent = 100.0;
  }

  if (this->battery_remaining_sensor_ != nullptr)
    this->battery_remaining_sensor_->publish_state(percent);

  this->status_clear_warning();
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
  ESP_LOGD(TAG, "MAX17043 CONFIG register reads 0x%X", config_reg);

  if (config_reg != MAX17043_CONFIG_POWER_UP_DEFAULT) {
    ESP_LOGE(TAG, "Device does not appear to be a MAX17043");
    this->status_set_error("unrecognised");
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
  ESP_LOGCONFIG(TAG, "  Voltage: %s", this->voltage_sensor_->get_name().c_str());
  ESP_LOGCONFIG(TAG, "  Percent: %s", this->battery_remaining_sensor_->get_name().c_str());
}

float MAX17043Component::get_setup_priority() const { return setup_priority::DATA; }

bool MAX17043Component::read_data_(uint16_t *raw_voltage, uint16_t *raw_percent) {
  if (this->write(&MAX17043_VCELL, 1) != i2c::ERROR_OK) {
    this->status_set_warning();
    return false;
  }

  if (this->read(reinterpret_cast<uint8_t *>(raw_voltage), 2) != i2c::ERROR_OK) {
    this->status_set_warning();
    return false;
  }
  *raw_voltage = i2c::i2ctohs(*raw_voltage);

  if (this->write(&MAX17043_SOC, 1) != i2c::ERROR_OK) {
    this->status_set_warning();
    return false;
  }

  if (this->read(reinterpret_cast<uint8_t *>(raw_percent), 2) != i2c::ERROR_OK) {
    this->status_set_warning();
    return false;
  }
  *raw_percent = i2c::i2ctohs(*raw_percent);

  return true;
}

}  // namespace max17043
}  // namespace esphome
