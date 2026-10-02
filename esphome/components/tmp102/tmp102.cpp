#include "tmp102.h"
#include "esphome/core/log.h"

#include <cstdio>
#include <cmath>

namespace esphome::tmp102 {

static const char *const TAG = "tmp102";

static const uint8_t TMP102_REGISTER_TEMPERATURE = 0x00;
static const uint8_t TMP102_REGISTER_CONFIGURATION = 0x01;
static const uint8_t TMP102_REGISTER_LOW_LIMIT = 0x02;
static const uint8_t TMP102_REGISTER_HIGH_LIMIT = 0x03;

static const float TMP102_CONVERSION_FACTOR = 0.0625f;
static const float TMP102_DEFAULT_THIGH = 80.0f;
static const float TMP102_DEFAULT_TLOW = 75.0f;

// Configuration register high byte bit positions
static const uint8_t TMP102_CFG_OS_BIT = 7;   // One-shot trigger
static const uint8_t TMP102_CFG_F0_BIT = 3;   // Fault queue LSB (F1:F0 at bits 4:3)
static const uint8_t TMP102_CFG_POL_BIT = 2;  // Alert polarity
static const uint8_t TMP102_CFG_TM_BIT = 1;   // Thermostat mode
static const uint8_t TMP102_CFG_SD_BIT = 0;   // Shutdown / one-shot enable

// Configuration register low byte bit positions
static const uint8_t TMP102_CFG_CR0_BIT = 6;  // Conversion rate LSB (CR1:CR0 at bits 7:6)
static const uint8_t TMP102_CFG_AL_BIT = 5;   // Alert status (read-only)
static const uint8_t TMP102_CFG_EM_BIT = 4;   // Extended mode

static const uint32_t TMP102_ONESHOT_DELAY_MS = 40;  // datasheet max 35 ms + 5 ms margin
// Persistent writable bits: FQ, POL, TM, SD, CR and EM. Excludes OS commands and read-only R/AL bits.
static const uint16_t TMP102_CONFIGURATION_MASK = 0x1FD0;

static float quantize_temperature(float temperature) {
  return std::round(temperature / TMP102_CONVERSION_FACTOR) * TMP102_CONVERSION_FACTOR;
}

static uint16_t encode_temperature(float temperature, bool extended) {
  const int16_t steps = static_cast<int16_t>(std::lround(temperature / TMP102_CONVERSION_FACTOR));
  return static_cast<uint16_t>(static_cast<uint16_t>(steps) << (extended ? 3 : 4));
}

void TMP102Component::setup() {
  // Upstream's temperature-only driver does not configure the chip at startup.
  if (!this->configure_) {
    this->setup_complete_ = true;
    return;
  }
  ESP_LOGCONFIG(TAG, "Setting up TMP102...");
  // Resolve both restored limits before validating their relationship or touching hardware.
  float high = this->get_limit_temperature(TMP102_LIMIT_HIGH);
  float low = this->get_limit_temperature(TMP102_LIMIT_LOW);
  bool corrected = false;
#ifdef USE_TMP102_NUMBER
  if (this->high_limit_control_ != nullptr)
    high = this->high_limit_control_->restore_value(corrected);
  if (this->low_limit_control_ != nullptr)
    low = this->low_limit_control_->restore_value(corrected);
#endif
  if (low > high) {
    ESP_LOGW(TAG, "Restored thresholds are inverted; using configured initial values");
    high = this->get_limit_temperature(TMP102_LIMIT_HIGH);
    low = this->get_limit_temperature(TMP102_LIMIT_LOW);
    corrected = true;
  }
  high = quantize_temperature(high);
  low = quantize_temperature(low);
  // Always write both limits in the selected format, including after an MCU-only reboot.
  if (!this->write_config_register_() || !this->write_limit_register_(TMP102_REGISTER_HIGH_LIMIT, high) ||
      !this->write_limit_register_(TMP102_REGISTER_LOW_LIMIT, low)) {
    this->mark_failed();
    return;
  }
  this->temperature_high_ = high;
  this->temperature_low_ = low;
  this->setup_complete_ = true;
  bool saved = true;
#ifdef USE_TMP102_NUMBER
  if (this->high_limit_control_ != nullptr)
    saved &= this->high_limit_control_->save_value(high);
  if (this->low_limit_control_ != nullptr)
    saved &= this->low_limit_control_->save_value(low);
#endif
  if (!saved) {
    ESP_LOGW(TAG, "Failed to save threshold preferences");
    this->publish_threshold_status(corrected ? "Invalid restored thresholds reset; preference save failed"
                                             : "Preference save failed");
  } else {
    this->publish_threshold_status(corrected ? "Invalid restored thresholds; reset to initial" : "OK");
  }
}

void TMP102Component::dump_config() {
  ESP_LOGCONFIG(TAG, "TMP102:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
  }
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Temperature", this);
#ifdef USE_TMP102_BINARY_SENSOR
  LOG_BINARY_SENSOR("  ", "Alert", this->alert_sensor_);
#endif
#ifdef USE_TMP102_TEXT_SENSOR
  LOG_TEXT_SENSOR("  ", "Threshold Status", this->threshold_status_sensor_);
#endif
  if (!this->configure_)
    return;
  ESP_LOGCONFIG(TAG, "  Extended Mode: %s", YESNO(this->extended_mode_));
  ESP_LOGCONFIG(TAG, "  One-Shot Mode: %s", YESNO(this->one_shot_mode_));
  const LogString *rate_str = this->conversion_rate_ == TMP102_CONVERSION_RATE_0_25HZ ? LOG_STR("0.25Hz")
                              : this->conversion_rate_ == TMP102_CONVERSION_RATE_1HZ  ? LOG_STR("1Hz")
                              : this->conversion_rate_ == TMP102_CONVERSION_RATE_4HZ  ? LOG_STR("4Hz")
                                                                                      : LOG_STR("8Hz");
  ESP_LOGCONFIG(TAG, "  Conversion Rate: %s", LOG_STR_ARG(rate_str));
  ESP_LOGCONFIG(TAG, "  Thermostat Mode: %s",
                this->thermostat_mode_ == TMP102_THERMOSTAT_MODE_COMPARATOR ? LOG_STR_LITERAL("Comparator")
                                                                            : LOG_STR_LITERAL("Interrupt"));
  ESP_LOGCONFIG(TAG, "  Alert Polarity: %s",
                this->alert_polarity_ == TMP102_ALERT_POLARITY_ACTIVE_LOW ? LOG_STR_LITERAL("Active Low")
                                                                          : LOG_STR_LITERAL("Active High"));
  ESP_LOGCONFIG(TAG, "  Fault Queue: %d", this->fault_queue_);
  if (this->temperature_high_.has_value()) {
    ESP_LOGCONFIG(TAG, "  Temperature High: %.1f°C", *this->temperature_high_);
  }
  if (this->temperature_low_.has_value()) {
    ESP_LOGCONFIG(TAG, "  Temperature Low: %.1f°C", *this->temperature_low_);
  }
#ifdef USE_TMP102_BINARY_SENSOR
  if (this->alert_sensor_ != nullptr && this->thermostat_mode_ == TMP102_THERMOSTAT_MODE_INTERRUPT) {
    ESP_LOGW(TAG, "Alert sensor reflects comparator status; register reads clear the interrupt-mode ALERT pin");
  }
#endif
}

void TMP102Component::update() {
  if (!this->setup_complete_ || this->conversion_pending_)
    return;
  // Also guard callbacks from recovery/status publications against recursive updates.
  this->conversion_pending_ = true;
  if (this->configure_ && !this->check_configuration_()) {
    this->conversion_pending_ = false;
    return;
  }
  if (this->one_shot_mode_) {
    // Trigger a single conversion: write config register with SD=1 and OS=1
    uint8_t frame[3] = {
        TMP102_REGISTER_CONFIGURATION,
        static_cast<uint8_t>(this->config_high_byte_ | (1 << TMP102_CFG_OS_BIT)),
        this->config_low_byte_,
    };
    if (this->write(frame, 3) != i2c::ERROR_OK) {
      this->read_failed_();
      this->conversion_pending_ = false;
      return;
    }
    // Conversion takes up to 35 ms; wait 40 ms then re-point and read
    this->set_timeout("read_temp", TMP102_ONESHOT_DELAY_MS, [this]() {
      this->read_temperature_();
      this->conversion_pending_ = false;
    });
  } else {
    this->read_temperature_();
    this->conversion_pending_ = false;
  }
}

bool TMP102Component::write_config_register_() {
  uint8_t high_byte = 0x00;
  uint8_t low_byte = 0x00;

  // Fault queue: 1→00, 2→01, 4→10, 6→11 (F1:F0 at bits 4:3)
  uint8_t fq_bits;
  switch (this->fault_queue_) {
    case 2:
      fq_bits = 0b01;
      break;
    case 4:
      fq_bits = 0b10;
      break;
    case 6:
      fq_bits = 0b11;
      break;
    default:
      fq_bits = 0b00;
      break;
  }
  high_byte |= static_cast<uint8_t>(fq_bits << TMP102_CFG_F0_BIT);

  if (this->alert_polarity_ == TMP102_ALERT_POLARITY_ACTIVE_HIGH) {
    high_byte |= static_cast<uint8_t>(1 << TMP102_CFG_POL_BIT);
  }

  if (this->thermostat_mode_ == TMP102_THERMOSTAT_MODE_INTERRUPT) {
    high_byte |= static_cast<uint8_t>(1 << TMP102_CFG_TM_BIT);
  }

  if (this->one_shot_mode_) {
    high_byte |= static_cast<uint8_t>(1 << TMP102_CFG_SD_BIT);  // SD=1 for shutdown/one-shot
  }

  // CR1:CR0 at bits 7:6
  low_byte |= static_cast<uint8_t>(static_cast<uint8_t>(this->conversion_rate_) << TMP102_CFG_CR0_BIT);

  if (this->extended_mode_) {
    low_byte |= static_cast<uint8_t>(1 << TMP102_CFG_EM_BIT);
  }

  // Cache for use in update() one-shot path
  this->config_high_byte_ = high_byte;
  this->config_low_byte_ = low_byte;

  uint8_t frame[3] = {TMP102_REGISTER_CONFIGURATION, high_byte, low_byte};
  if (this->write(frame, 3) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
    return false;
  }
  return true;
}

bool TMP102Component::write_limit_register_(uint8_t reg, float temperature) {
  const uint16_t raw = encode_temperature(temperature, this->extended_mode_);
  uint8_t frame[3] = {
      reg,
      static_cast<uint8_t>((raw >> 8) & 0xFF),
      static_cast<uint8_t>(raw & 0xFF),
  };
  if (this->write(frame, 3) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
    return false;
  }
  return true;
}

void TMP102Component::publish_threshold_status(const char *status) {
#ifdef USE_TMP102_TEXT_SENSOR
  if (this->threshold_status_sensor_ != nullptr) {
    this->threshold_status_sensor_->publish_state(status);
  }
#endif
}

bool TMP102Component::validate_limit_temperature_(TMP102LimitType limit, float temperature) {
  if ((limit != TMP102_LIMIT_HIGH && limit != TMP102_LIMIT_LOW) || !std::isfinite(temperature) ||
      temperature < -55.0f || temperature > (this->extended_mode_ ? 150.0f : 127.9375f)) {
    ESP_LOGW(TAG, "Rejected invalid threshold");
    this->publish_threshold_status("Rejected: threshold out of range");
    return false;
  }
  temperature = quantize_temperature(temperature);
  char status[96];
  if (limit == TMP102_LIMIT_HIGH && temperature < this->get_limit_temperature(TMP102_LIMIT_LOW)) {
    std::snprintf(status, sizeof(status), "Rejected: high %.2f°C below clear %.2f°C", temperature,
                  this->get_limit_temperature(TMP102_LIMIT_LOW));
    ESP_LOGW(TAG, "%s", status);
    this->publish_threshold_status(status);
    return false;
  }
  if (limit == TMP102_LIMIT_LOW && temperature > this->get_limit_temperature(TMP102_LIMIT_HIGH)) {
    std::snprintf(status, sizeof(status), "Rejected: clear %.2f°C above high %.2f°C", temperature,
                  this->get_limit_temperature(TMP102_LIMIT_HIGH));
    ESP_LOGW(TAG, "%s", status);
    this->publish_threshold_status(status);
    return false;
  }
  return true;
}

bool TMP102Component::set_limit_temperature(TMP102LimitType limit, float temperature) {
  if (this->is_failed()) {
    ESP_LOGW(TAG, "Rejected threshold change: sensor setup failed");
    this->publish_threshold_status("Rejected: sensor failed");
    return false;
  }
  if (!this->validate_limit_temperature_(limit, temperature)) {
    return false;
  }
  temperature = quantize_temperature(temperature);

  const uint8_t reg = limit == TMP102_LIMIT_HIGH ? TMP102_REGISTER_HIGH_LIMIT : TMP102_REGISTER_LOW_LIMIT;
  if (this->setup_complete_) {
    if (!this->write_limit_register_(reg, temperature)) {
      this->threshold_write_failed_ = true;
      this->status_set_warning();
      this->publish_threshold_status(limit == TMP102_LIMIT_HIGH ? "Write failed: high threshold"
                                                                : "Write failed: clear threshold");
      return false;
    }
    this->threshold_write_failed_ = false;
  }

  if (limit == TMP102_LIMIT_HIGH) {
    this->temperature_high_ = temperature;
  } else {
    this->temperature_low_ = temperature;
  }

  ESP_LOGI(TAG, "%s set to %.2f°C", limit == TMP102_LIMIT_HIGH ? LOG_STR_LITERAL("THIGH") : LOG_STR_LITERAL("TLOW"),
           temperature);
  this->publish_threshold_status("OK");
  return true;
}

float TMP102Component::get_limit_temperature(TMP102LimitType limit) const {
  if (limit == TMP102_LIMIT_HIGH) {
    return this->temperature_high_.value_or(TMP102_DEFAULT_THIGH);
  }
  return this->temperature_low_.value_or(TMP102_DEFAULT_TLOW);
}

bool TMP102Component::check_configuration_() {
  // A temperature format change alone may be a conversion in flight. Check the actual writable registers.
  uint16_t config, high, low;
  if (!this->read_byte_16(TMP102_REGISTER_CONFIGURATION, &config) ||
      !this->read_byte_16(TMP102_REGISTER_HIGH_LIMIT, &high) || !this->read_byte_16(TMP102_REGISTER_LOW_LIMIT, &low)) {
    this->read_failed_();
    return false;
  }
  const uint16_t expected = (static_cast<uint16_t>(this->config_high_byte_) << 8) | this->config_low_byte_;
  if ((config & TMP102_CONFIGURATION_MASK) == (expected & TMP102_CONFIGURATION_MASK) &&
      high == encode_temperature(this->get_limit_temperature(TMP102_LIMIT_HIGH), this->extended_mode_) &&
      low == encode_temperature(this->get_limit_temperature(TMP102_LIMIT_LOW), this->extended_mode_))
    return true;

  ESP_LOGW(TAG, "Configuration changed; restoring configured settings and thresholds");
  this->read_failed_();
  if (!this->write_config_register_() ||
      !this->write_limit_register_(TMP102_REGISTER_HIGH_LIMIT, this->get_limit_temperature(TMP102_LIMIT_HIGH)) ||
      !this->write_limit_register_(TMP102_REGISTER_LOW_LIMIT, this->get_limit_temperature(TMP102_LIMIT_LOW))) {
    this->publish_threshold_status("Configuration recovery failed");
  } else {
    this->publish_threshold_status("Configuration restored");
  }
  // Defer publication until the next update. A subsequent read uses the format flag in the sample itself.
  return false;
}

void TMP102Component::read_failed_() {
  this->status_set_warning();
#ifdef USE_TMP102_BINARY_SENSOR
  if (this->alert_sensor_ != nullptr)
    this->alert_sensor_->invalidate_state();
#endif
}

void TMP102Component::read_temperature_() {
  // Select immediately before reading: another operation may have changed the pointer.
  if (this->write(&TMP102_REGISTER_TEMPERATURE, 1) != i2c::ERROR_OK) {
    this->read_failed_();
    return;
  }

  int16_t raw_temperature;
  if (this->read(reinterpret_cast<uint8_t *>(&raw_temperature), 2) != i2c::ERROR_OK) {
    this->read_failed_();
    return;
  }
  raw_temperature = i2c::i2ctohs(raw_temperature);
  // Extended mode: 13-bit value in upper 13 bits → right-shift 3
  // Normal mode: 12-bit value in upper 12 bits → right-shift 4
  raw_temperature = (raw_temperature & 0x0001) ? (raw_temperature >> 3) : (raw_temperature >> 4);
  float temperature = raw_temperature * TMP102_CONVERSION_FACTOR;
  ESP_LOGD(TAG, "Got Temperature=%.2f°C", temperature);
  this->publish_state(temperature);

#ifdef USE_TMP102_BINARY_SENSOR
  if (this->alert_sensor_ != nullptr && !this->read_alert_state_())
    return;
#endif
  if (!this->threshold_write_failed_)
    this->status_clear_warning();
}

#ifdef USE_TMP102_BINARY_SENSOR
bool TMP102Component::read_alert_state_() {
  if (this->write(&TMP102_REGISTER_CONFIGURATION, 1) != i2c::ERROR_OK) {
    this->read_failed_();
    ESP_LOGW(TAG, "Failed to select config register for AL bit read");
    return false;
  }
  uint8_t cfg[2];
  if (this->read(cfg, 2) != i2c::ERROR_OK) {
    this->read_failed_();
    ESP_LOGW(TAG, "Failed to read config register for AL bit");
    return false;
  }
  // AL is bit 5 of the low byte (cfg[1]).
  // When POL=0 (active low): chip sets AL=0 when alert is active → alert_active = (AL==0).
  // When POL=1 (active high): chip sets AL=1 when alert is active → alert_active = (AL==1).
  // In both cases: alert_active = (AL_bit == POL_value).
  // AL reports comparator status regardless of TM; register reads only clear the interrupt pin.
  uint8_t al_bit = (cfg[1] >> TMP102_CFG_AL_BIT) & 0x01;
  const uint8_t polarity = (cfg[0] >> TMP102_CFG_POL_BIT) & 0x01;
  bool alert_active = (al_bit == polarity);
  ESP_LOGD(TAG, "AL=%d alert_active=%s", al_bit, YESNO(alert_active));
  this->alert_sensor_->publish_state(alert_active);
  return true;
}

#endif

#ifdef USE_TMP102_NUMBER
float TMP102LimitNumber::restore_value(bool &corrected) {
  float value = this->initial_value_;
  if (this->restore_value_) {
    this->pref_ = this->make_entity_preference<float>();
    if (!this->pref_.load(&value))
      value = this->initial_value_;
  }
  if (!std::isfinite(value) || value < this->traits.get_min_value() || value > this->traits.get_max_value()) {
    ESP_LOGW(TAG, "Invalid restored threshold; using initial value");
    value = this->initial_value_;
    corrected = true;
  }
  return value;
}

bool TMP102LimitNumber::save_value(float value) { return !this->restore_value_ || this->pref_.save(&value); }

void TMP102LimitNumber::setup() {
  if (this->parent_->is_failed()) {
    this->mark_failed();
    return;
  }
  this->publish_state(this->parent_->get_limit_temperature(this->limit_type_));
}

void TMP102LimitNumber::control(float value) {
  this->cancel_timeout("revert");
  if (this->parent_ != nullptr && this->parent_->set_limit_temperature(this->limit_type_, value)) {
    value = this->parent_->get_limit_temperature(this->limit_type_);
    this->publish_state(value);
    if (!this->save_value(value)) {
      ESP_LOGW(TAG, "Failed to save threshold preference");
      this->parent_->publish_threshold_status("Preference save failed");
    }
    return;
  }
  if (this->parent_ != nullptr) {
    const float current = this->parent_->get_limit_temperature(this->limit_type_);
    this->publish_state(current);
    this->set_timeout("revert", 250,
                      [this]() { this->publish_state(this->parent_->get_limit_temperature(this->limit_type_)); });
  }
}

void TMP102LimitNumber::dump_config() {
  const char *type = this->limit_type_ == TMP102_LIMIT_HIGH ? LOG_STR_LITERAL("TMP102 High Limit")
                                                            : LOG_STR_LITERAL("TMP102 Low Limit");
  number::log_number(TAG, "", type, this);
  ESP_LOGCONFIG(TAG, "  Restore Value: %s", YESNO(this->restore_value_));
}

#endif

}  // namespace esphome::tmp102
