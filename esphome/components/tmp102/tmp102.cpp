#include "tmp102.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome::tmp102 {

ESPHOME_LOG_TAG(TAG, "tmp102");

static constexpr uint32_t READ_TEMP_TIMEOUT_ID = 0;

static const uint8_t TMP102_REGISTER_TEMPERATURE = 0x00;
static const float TMP102_CONVERSION_FACTOR = 0.0625f;
#ifdef USE_TMP102_CONFIGURE
static const uint8_t TMP102_REGISTER_CONFIGURATION = 0x01;
static const uint8_t TMP102_REGISTER_LOW_LIMIT = 0x02;
static const uint8_t TMP102_REGISTER_HIGH_LIMIT = 0x03;

static const uint16_t TMP102_CONFIGURATION_MASK = 0x1FD0;
static const uint16_t TMP102_ONE_SHOT_BIT = 0x8000;
static const uint16_t TMP102_SHUTDOWN_BIT = 0x0100;
static const uint16_t TMP102_THERMOSTAT_MODE_BIT = 0x0200;
static const uint16_t TMP102_ALERT_POLARITY_BIT = 0x0400;
static const uint16_t TMP102_EXTENDED_MODE_BIT = 0x0010;
static const uint32_t TMP102_ONESHOT_DELAY_MS = 40;

static float decode_limit(uint16_t raw, bool extended) {
  const int16_t value = static_cast<int16_t>(raw) >> (extended ? 3 : 4);
  return value * 0.0625f;
}

void TMP102Component::setup() {
  if (!this->configure_) {
    this->setup_complete_ = true;
    return;
  }
  ESP_LOGCONFIG(TAG, "Setting up TMP102...");
  if (!this->write_register_(TMP102_REGISTER_CONFIGURATION, this->config_) ||
      !this->write_register_(TMP102_REGISTER_HIGH_LIMIT, this->high_limit_) ||
      !this->write_register_(TMP102_REGISTER_LOW_LIMIT, this->low_limit_)) {
    this->mark_failed();
    return;
  }
  this->setup_complete_ = true;
}
#endif

void TMP102Component::dump_config() {
  ESP_LOGCONFIG(TAG, "TMP102:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
  }
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Temperature", this);
#ifdef USE_TMP102_CONFIGURE
  if (!this->configure_)
    return;
  const uint8_t rate = (this->config_ >> 6) & 0x03;
  const LogString *rate_str = rate == 0   ? LOG_STR("0.25Hz")
                              : rate == 1 ? LOG_STR("1Hz")
                              : rate == 2 ? LOG_STR("4Hz")
                                          : LOG_STR("8Hz");
  ESP_LOGCONFIG(TAG, "  Extended Mode: %s", YESNO(this->config_ & TMP102_EXTENDED_MODE_BIT));
  ESP_LOGCONFIG(TAG, "  One-Shot Mode: %s", YESNO(this->config_ & TMP102_SHUTDOWN_BIT));
  ESP_LOGCONFIG(TAG, "  Conversion Rate: %s", LOG_STR_ARG(rate_str));
  ESP_LOGCONFIG(
      TAG, "  Thermostat Mode: %s",
      this->config_ & TMP102_THERMOSTAT_MODE_BIT ? LOG_STR_LITERAL("Interrupt") : LOG_STR_LITERAL("Comparator"));
  ESP_LOGCONFIG(
      TAG, "  Alert Polarity: %s",
      this->config_ & TMP102_ALERT_POLARITY_BIT ? LOG_STR_LITERAL("Active High") : LOG_STR_LITERAL("Active Low"));
  const uint8_t fault_bits = (this->config_ >> 11) & 0x03;
  ESP_LOGCONFIG(TAG, "  Fault Queue: %u", fault_bits == 3 ? 6 : 1U << fault_bits);
  const bool extended = this->config_ & TMP102_EXTENDED_MODE_BIT;
  if (this->configured_limits_ & 0x02) {
    ESP_LOGCONFIG(TAG, "  Temperature High: %.4f°C", decode_limit(this->high_limit_, extended));
  }
  if (this->configured_limits_ & 0x01) {
    ESP_LOGCONFIG(TAG, "  Temperature Low: %.4f°C", decode_limit(this->low_limit_, extended));
  }
  if (this->config_ & TMP102_THERMOSTAT_MODE_BIT) {
    ESP_LOGW(TAG, "Register reads clear the interrupt-mode ALERT pin");
  }
#endif
}

void TMP102Component::update() {
#ifndef USE_TMP102_CONFIGURE
  if (this->write(&TMP102_REGISTER_TEMPERATURE, 1) != i2c::ERROR_OK) {
    this->status_set_warning();
    return;
  }
  this->set_timeout(READ_TEMP_TIMEOUT_ID, 50, [this]() {
    int16_t raw_temperature;
    if (this->read(reinterpret_cast<uint8_t *>(&raw_temperature), 2) != i2c::ERROR_OK) {
      this->status_set_warning();
      return;
    }
    raw_temperature = i2c::i2ctohs(raw_temperature);
    raw_temperature = raw_temperature >> 4;
    float temperature = raw_temperature * TMP102_CONVERSION_FACTOR;
    ESP_LOGD(TAG, "Got Temperature=%.1f°C", temperature);

    this->publish_state(temperature);
    this->status_clear_warning();
  });
#else
  if (!this->configure_) {
    if (this->write(&TMP102_REGISTER_TEMPERATURE, 1) != i2c::ERROR_OK) {
      this->status_set_warning();
      return;
    }
    this->set_timeout(READ_TEMP_TIMEOUT_ID, 50, [this]() {
      int16_t raw_temperature;
      if (this->read(reinterpret_cast<uint8_t *>(&raw_temperature), 2) != i2c::ERROR_OK) {
        this->status_set_warning();
        return;
      }
      raw_temperature = i2c::i2ctohs(raw_temperature) >> 4;
      this->publish_state(raw_temperature * TMP102_CONVERSION_FACTOR);
      this->status_clear_warning();
    });
    return;
  }
  if (!this->setup_complete_ || this->conversion_pending_)
    return;
  this->conversion_pending_ = true;
  if (!this->check_configuration_()) {
    this->conversion_pending_ = false;
    return;
  }
  if (this->config_ & TMP102_SHUTDOWN_BIT) {
    if (!this->write_register_(TMP102_REGISTER_CONFIGURATION, this->config_ | TMP102_ONE_SHOT_BIT)) {
      this->status_set_warning();
      this->conversion_pending_ = false;
      return;
    }
    this->set_timeout(READ_TEMP_TIMEOUT_ID, TMP102_ONESHOT_DELAY_MS, [this]() {
      this->read_temperature_();
      this->conversion_pending_ = false;
    });
  } else {
    this->read_temperature_();
    this->conversion_pending_ = false;
  }
#endif
}

#ifdef USE_TMP102_CONFIGURE
bool TMP102Component::write_register_(uint8_t reg, uint16_t value) {
  uint8_t frame[3] = {reg, static_cast<uint8_t>(value >> 8), static_cast<uint8_t>(value)};
  if (this->write(frame, 3) == i2c::ERROR_OK)
    return true;
  ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
  return false;
}

bool TMP102Component::check_configuration_() {
  uint16_t config, high, low;
  if (!this->read_byte_16(TMP102_REGISTER_CONFIGURATION, &config) ||
      !this->read_byte_16(TMP102_REGISTER_HIGH_LIMIT, &high) || !this->read_byte_16(TMP102_REGISTER_LOW_LIMIT, &low)) {
    this->status_set_warning();
    return false;
  }
  if ((config & TMP102_CONFIGURATION_MASK) == (this->config_ & TMP102_CONFIGURATION_MASK) &&
      high == this->high_limit_ && low == this->low_limit_)
    return true;

  ESP_LOGW(TAG, "Configuration changed; restoring configured settings and thresholds");
  this->status_set_warning();
  if (!this->write_register_(TMP102_REGISTER_CONFIGURATION, this->config_) ||
      !this->write_register_(TMP102_REGISTER_HIGH_LIMIT, this->high_limit_) ||
      !this->write_register_(TMP102_REGISTER_LOW_LIMIT, this->low_limit_)) {
    ESP_LOGW(TAG, "Failed to restore configuration and thresholds");
  }
  return false;
}

void TMP102Component::read_temperature_() {
  if (this->write(&TMP102_REGISTER_TEMPERATURE, 1) != i2c::ERROR_OK) {
    this->status_set_warning();
    return;
  }
  int16_t raw_temperature;
  if (this->read(reinterpret_cast<uint8_t *>(&raw_temperature), 2) != i2c::ERROR_OK) {
    this->status_set_warning();
    return;
  }
  raw_temperature = i2c::i2ctohs(raw_temperature);
  raw_temperature = (raw_temperature & 0x0001) ? (raw_temperature >> 3) : (raw_temperature >> 4);
  const float temperature = raw_temperature * TMP102_CONVERSION_FACTOR;
  ESP_LOGD(TAG, "Got Temperature=%.2f°C", temperature);
  this->publish_state(temperature);
  this->status_clear_warning();
}
#endif

}  // namespace esphome::tmp102
