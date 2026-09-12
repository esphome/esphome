#include "max17043.h"
#include "esphome/core/log.h"

namespace esphome::max17043 {

// MAX17043 is a 1-Cell Fuel Gauge with ModelGauge and Low-Battery Alert
// Consult the datasheet at https://www.analog.com/en/products/max17043.html
// The MAX17048 is a register-compatible successor with a finer VCELL LSb, an extra
// charge-rate register, and a MODE.EnSleep gate in front of CONFIG.SLEEP.
// Consult the datasheet at https://www.analog.com/en/products/max17048.html

static const char *const TAG = "max17043";

static const uint8_t MAX17043_VCELL = 0x02;
static const uint8_t MAX17043_SOC = 0x04;
static const uint8_t MAX17043_MODE = 0x06;
static const uint8_t MAX17043_VERSION = 0x08;
static const uint8_t MAX17043_CONFIG = 0x0c;
static const uint8_t MAX17048_CRATE = 0x16;

static const uint16_t MAX17043_CONFIG_POWER_UP_DEFAULT = 0x971C;
// Mask out SLEEP (7), ALRT (5), and bit 6, which is a don't-care on the MAX17043 but ALSC on the MAX17048.
static const uint16_t MAX17043_CONFIG_SAFE_MASK = 0xFF1F;
static const uint16_t MAX17043_CONFIG_SLEEP_MASK = 0x0080;

// MAX17048 only: CONFIG.SLEEP is ignored unless MODE.EnSleep (bit 13) is set first.
static const uint16_t MAX17048_MODE_EN_SLEEP = 0x2000;

// MAX17048 only: CRATE has an LSb of 0.208%/hr.
static const float MAX17048_CRATE_LSB = 0.208f;

// Boards that gate the I2C rail can leave the bus unusable until after this component has
// been set up, so a silent bus at setup is retried rather than treated as a missing device.
// Each failed probe blocks for the I2C driver timeout (~100ms), so the interval is kept well
// above that to bound how much of the main loop the retries can consume while booting.
static const char *const CONFIGURE_RETRY = "configure";
static const uint32_t CONFIGURE_RETRY_INTERVAL_MS = 500;
static const uint8_t CONFIGURE_MAX_ATTEMPTS = 10;

void MAX17043Component::update() {
  uint16_t raw_voltage, raw_percent, raw_charge_rate;

  if (this->voltage_sensor_ != nullptr) {
    if (!this->read_byte_16(MAX17043_VCELL, &raw_voltage)) {
      this->status_set_warning(LOG_STR("Unable to read MAX17043_VCELL"));
    } else {
      // Each part is scaled by its own datasheet LSb: 1.25mV on the MAX17043, whose 12-bit
      // reading sits in the top bits of the word, and 78.125uV on the MAX17048. Effective
      // resolution is 1.25mV on both -- the MAX17048 ADC is 12-bit as well and its register
      // holds that reading scaled by 16 -- so the two expressions agree for every value the
      // hardware produces. Keeping them separate avoids depending on that low nibble being
      // zero, which the register format does not promise.
      float voltage = this->is_max17048_() ? (78.125f * (float) raw_voltage) / 1000000.0f
                                           : (1.25f * (float) (raw_voltage >> 4)) / 1000.0f;
      this->voltage_sensor_->publish_state(voltage);
      this->status_clear_warning();
    }
  }
  if (this->battery_remaining_sensor_ != nullptr) {
    if (!this->read_byte_16(MAX17043_SOC, &raw_percent)) {
      this->status_set_warning(LOG_STR("Unable to read MAX17043_SOC"));
    } else {
      float percent = (float) ((raw_percent >> 8) + 0.003906f * (raw_percent & 0x00ff));
      this->battery_remaining_sensor_->publish_state(percent);
      this->status_clear_warning();
    }
  }
  if (this->charge_rate_sensor_ != nullptr) {
    if (!this->read_byte_16(MAX17048_CRATE, &raw_charge_rate)) {
      this->status_set_warning(LOG_STR("Unable to read MAX17048_CRATE"));
    } else {
      // CRATE is two's complement: negative while the battery is discharging.
      this->charge_rate_sensor_->publish_state((float) (int16_t) raw_charge_rate * MAX17048_CRATE_LSB);
      this->status_clear_warning();
    }
  }
}

void MAX17043Component::setup() {
  if (this->configure_())
    return;

  // The gauge did not answer. On boards that gate the I2C rail (for example the I2C_POWER
  // pin on Adafruit Feathers) the bus only becomes usable after this component is set up,
  // so suspend polling and keep trying instead of reporting from a chip we never verified.
  ESP_LOGD(TAG, "Gauge did not answer, deferring configuration");
  this->stop_poller();
  this->set_interval(CONFIGURE_RETRY, CONFIGURE_RETRY_INTERVAL_MS, [this]() {
    if (this->is_failed()) {
      this->cancel_interval(CONFIGURE_RETRY);
      return;
    }
    if (this->configure_()) {
      this->cancel_interval(CONFIGURE_RETRY);
      this->start_poller();
      return;
    }
    if (++this->configure_attempts_ >= CONFIGURE_MAX_ATTEMPTS) {
      // Give up configuring, but still report: VCELL and SOC do not depend on the write-back,
      // so polling anyway is never worse than not polling at all.
      this->cancel_interval(CONFIGURE_RETRY);
      ESP_LOGW(TAG, "Gauge never answered; polling anyway, any sleep bit is left set");
      this->status_set_warning(LOG_STR("not configured"));
      this->start_poller();
    }
  });
}

bool MAX17043Component::configure_() {
  uint16_t config_reg;
  if (this->write(&MAX17043_CONFIG, 1) != i2c::ERROR_OK)
    return false;

  if (this->read(reinterpret_cast<uint8_t *>(&config_reg), 2) != i2c::ERROR_OK)
    return false;

  config_reg = i2c::i2ctohs(config_reg) & MAX17043_CONFIG_SAFE_MASK;
  ESP_LOGV(TAG, "MAX17043 CONFIG register reads 0x%X", config_reg);

  // Both models power up with the same CONFIG value, so this only proves that something
  // fuel-gauge shaped answered; it cannot tell the two of them apart.
  if (config_reg != MAX17043_CONFIG_POWER_UP_DEFAULT) {
    ESP_LOGE(TAG, "Device does not appear to be a %s", this->model_name_());
    this->status_set_error(LOG_STR("unrecognised"));
    this->mark_failed();
    return false;
  }

  // Informational only. Both parts expose a production version at 0x08, but the values are
  // not documented as a dependable way to distinguish them, so this never fails setup. It is
  // logged here rather than in dump_config() because configuration can be deferred past it.
  if (this->read_byte_16(MAX17043_VERSION, &this->version_)) {
    ESP_LOGCONFIG(TAG, "%s IC version: 0x%04X", this->model_name_(), this->version_);
  } else {
    ESP_LOGW(TAG, "Unable to read the version register");
  }

  // need to write back to config register to reset the sleep bit
  if (!this->write_byte_16(MAX17043_CONFIG, MAX17043_CONFIG_POWER_UP_DEFAULT)) {
    this->status_set_error(LOG_STR("sleep reset failed"));
    this->mark_failed();
    return false;
  }

  this->configured_ = true;
  this->status_clear_warning();
  return true;
}

void MAX17043Component::dump_config() {
  ESP_LOGCONFIG(TAG, "MAX17043:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
  }
  ESP_LOGCONFIG(TAG, "  Model: %s", this->model_name_());
  if (this->configured_) {
    ESP_LOGCONFIG(TAG, "  IC version: 0x%04X", this->version_);
  } else {
    ESP_LOGCONFIG(TAG, "  IC version: unread, configuration deferred");
  }
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Battery Voltage", this->voltage_sensor_);
  LOG_SENSOR("  ", "Battery Level", this->battery_remaining_sensor_);
  LOG_SENSOR("  ", "Battery Charge Rate", this->charge_rate_sensor_);
}

void MAX17043Component::sleep_mode() {
  if (this->is_failed()) {
    return;
  }

  // On the MAX17048 the sleep bit in CONFIG does nothing until MODE.EnSleep is set.
  if (this->is_max17048_() && !this->write_byte_16(MAX17043_MODE, MAX17048_MODE_EN_SLEEP)) {
    ESP_LOGW(TAG, "Unable to enable sleep mode in the mode register");
    this->status_set_warning();
    return;
  }

  if (!this->write_byte_16(MAX17043_CONFIG, MAX17043_CONFIG_POWER_UP_DEFAULT | MAX17043_CONFIG_SLEEP_MASK)) {
    ESP_LOGW(TAG, "Unable to write the sleep bit to config register");
    this->status_set_warning();
  }
}

}  // namespace esphome::max17043
