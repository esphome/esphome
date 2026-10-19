#include "max17043.h"
#include "esphome/core/log.h"

namespace esphome::max17043 {

// MAX17043 is a 1-Cell Fuel Gauge with ModelGauge and Low-Battery Alert
// Consult the datasheet at https://www.analog.com/en/products/max17043.html
// The MAX17048 is a register-compatible successor with an extra charge-rate register
// and a MODE.EnSleep gate in front of CONFIG.SLEEP.
// Consult the datasheet at https://www.analog.com/en/products/max17048.html

ESPHOME_LOG_TAG(TAG, "max17043");

static constexpr uint8_t MAX17043_VCELL = 0x02;
static constexpr uint8_t MAX17043_SOC = 0x04;
static constexpr uint8_t MAX17043_VERSION = 0x08;
static constexpr uint8_t MAX17043_CONFIG = 0x0c;

static constexpr uint16_t MAX17043_CONFIG_POWER_UP_DEFAULT = 0x971C;
// Mask out SLEEP (7), ALRT (5), and bit 6, which is a don't-care on the MAX17043 but ALSC on the MAX17048.
static constexpr uint16_t MAX17043_CONFIG_SAFE_MASK = 0xFF1F;
static constexpr uint16_t MAX17043_CONFIG_SLEEP_MASK = 0x0080;

#ifdef USE_MAX17048
static constexpr uint8_t MAX17043_MODE = 0x06;
static constexpr uint8_t MAX17048_CRATE = 0x16;
// CONFIG.SLEEP is ignored unless MODE.EnSleep (bit 13) is set first.
static constexpr uint16_t MAX17048_MODE_EN_SLEEP = 0x2000;
static constexpr float MAX17048_CRATE_LSB = 0.208f;  // %/hr
#endif

void MAX17043Component::update() {
  if (!this->configured_ && !this->configure_()) {
    // A wrong CONFIG value has already failed the component with its own reason
    if (!this->is_failed())
      this->status_set_warning(LOG_STR("Gauge did not answer"));
    return;
  }

  uint16_t raw;
  bool ok = true;

  if (this->voltage_sensor_ != nullptr) {
    if (this->read_byte_16(MAX17043_VCELL, &raw)) {
      // MAX17043: 12 bits in the top of the word at 1.25 mV; MAX17048: 16 bits at 78.125 uV
      float voltage = (raw >> 4) * 1.25e-3f;
#ifdef USE_MAX17048
      if (this->is_max17048_())
        voltage = raw * 78.125e-6f;
#endif
      this->voltage_sensor_->publish_state(voltage);
    } else {
      ok = false;
    }
  }
  if (this->battery_remaining_sensor_ != nullptr) {
    if (this->read_byte_16(MAX17043_SOC, &raw)) {
      float percent = (float) ((raw >> 8) + 0.003906f * (raw & 0x00ff));
      this->battery_remaining_sensor_->publish_state(percent);
    } else {
      ok = false;
    }
  }
#ifdef USE_MAX17048
  if (this->charge_rate_sensor_ != nullptr) {
    if (this->read_byte_16(MAX17048_CRATE, &raw)) {
      // CRATE is two's complement: negative while the battery is discharging.
      this->charge_rate_sensor_->publish_state((float) (int16_t) raw * MAX17048_CRATE_LSB);
    } else {
      ok = false;
    }
  }
#endif

  this->status_comm_result(ok, LOG_STR("Register read failed"));
}

void MAX17043Component::setup() {
  // A silent bus is retried from update(): some boards gate the I2C rail until after setup.
  this->configure_();
}

bool MAX17043Component::configure_() {
  uint16_t config_reg;
  if (!this->read_byte_16(MAX17043_CONFIG, &config_reg))
    return false;

  config_reg &= MAX17043_CONFIG_SAFE_MASK;
  if (config_reg != MAX17043_CONFIG_POWER_UP_DEFAULT) {
    ESP_LOGE(TAG, "CONFIG reads 0x%04X, expected 0x%04X", config_reg, MAX17043_CONFIG_POWER_UP_DEFAULT);
    this->mark_failed(LOG_STR("Device not recognised"));
    return false;
  }

  uint16_t version;
  if (this->read_byte_16(MAX17043_VERSION, &version)) {
    ESP_LOGCONFIG(TAG, "IC version: 0x%04X", version);
  }

  // Write back the power-up value to clear a stale sleep bit; a failed write is retried like a silent bus
  if (!this->write_byte_16(MAX17043_CONFIG, MAX17043_CONFIG_POWER_UP_DEFAULT))
    return false;

  this->configured_ = true;
  return true;
}

void MAX17043Component::dump_config() {
  ESP_LOGCONFIG(TAG,
                "MAX17043:\n"
                "  Model: %s",
                LOG_STR_ARG(this->model_name_()));
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Battery Voltage", this->voltage_sensor_);
  LOG_SENSOR("  ", "Battery Level", this->battery_remaining_sensor_);
#ifdef USE_MAX17048
  LOG_SENSOR("  ", "Battery Charge Rate", this->charge_rate_sensor_);
#endif
}

void MAX17043Component::sleep_mode() {
  if (this->is_failed()) {
    return;
  }
  // The deferred configuration clears the sleep bit, so it has to run before the gauge is put to sleep
  if (!this->configured_ && !this->configure_()) {
    if (!this->is_failed()) {
      ESP_LOGW(TAG, "Gauge did not answer, not putting it to sleep");
      this->status_set_warning(LOG_STR("Gauge did not answer"));
    }
    return;
  }

#ifdef USE_MAX17048
  if (this->is_max17048_() && !this->write_byte_16(MAX17043_MODE, MAX17048_MODE_EN_SLEEP)) {
    ESP_LOGW(TAG, "Unable to enable sleep mode in the mode register");
    this->status_set_warning();
    return;
  }
#endif

  if (!this->write_byte_16(MAX17043_CONFIG, MAX17043_CONFIG_POWER_UP_DEFAULT | MAX17043_CONFIG_SLEEP_MASK)) {
    ESP_LOGW(TAG, "Unable to write the sleep bit to config register");
    this->status_set_warning();
  }
}

}  // namespace esphome::max17043
