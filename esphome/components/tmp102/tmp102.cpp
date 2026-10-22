#include "tmp102.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#ifdef USE_TMP102_TEXT_SENSOR
#include "esphome/core/progmem.h"
#endif

#include <cmath>
#ifdef USE_TMP102_TEXT_SENSOR
#include <cstdio>
#endif

namespace esphome::tmp102 {

ESPHOME_LOG_TAG(TAG, "tmp102");

static constexpr uint32_t READ_TEMP_TIMEOUT_ID = 0;

static constexpr uint8_t TMP102_REGISTER_TEMPERATURE = 0x00;
static constexpr float TMP102_CONVERSION_FACTOR = 0.0625f;

enum TMP102ThresholdStatus : uint8_t {
  TMP102_THRESHOLD_STATUS_NOT_CONFIGURED,
  TMP102_THRESHOLD_STATUS_SETUP_FAILED,
  TMP102_THRESHOLD_STATUS_OK,
  TMP102_THRESHOLD_STATUS_RESTORE_RESET,
  TMP102_THRESHOLD_STATUS_RESTORE_RESET_SAVE_FAILED,
  TMP102_THRESHOLD_STATUS_PREFERENCE_SAVE_FAILED,
  TMP102_THRESHOLD_STATUS_RECOVERY_FAILED,
  TMP102_THRESHOLD_STATUS_RECOVERED,
  TMP102_THRESHOLD_STATUS_WRITE_HIGH_FAILED,
  TMP102_THRESHOLD_STATUS_WRITE_LOW_FAILED,
  TMP102_THRESHOLD_STATUS_SENSOR_SETUP_FAILED,
  TMP102_THRESHOLD_STATUS_COUNT,
};

#ifdef USE_TMP102_TEXT_SENSOR
PROGMEM_STRING_TABLE(ThresholdStatusStrings, "Not configured", "Setup failed", "OK", "Restored thresholds reset",
                     "Restore reset; save failed", "Preference save failed", "Configuration recovery failed",
                     "Configuration restored", "Write failed: high threshold", "Write failed: low threshold",
                     "Rejected: sensor setup failed");
static_assert(ThresholdStatusStrings::COUNT == TMP102_THRESHOLD_STATUS_COUNT);
#endif

#if defined(USE_TMP102_CONFIGURE) || defined(USE_TMP102_BINARY_SENSOR)
static constexpr uint8_t TMP102_REGISTER_CONFIGURATION = 0x01;
static constexpr uint16_t TMP102_ALERT_BIT = 0x0020;
static constexpr uint16_t TMP102_ALERT_POLARITY_BIT = 0x0400;
#endif
#ifdef USE_TMP102_CONFIGURE
static constexpr uint8_t TMP102_REGISTER_LOW_LIMIT = 0x02;
static constexpr uint8_t TMP102_REGISTER_HIGH_LIMIT = 0x03;
#ifdef USE_TMP102_NUMBER
static_assert(TMP102_REGISTER_LOW_LIMIT + TMP102_LIMIT_HIGH == TMP102_REGISTER_HIGH_LIMIT);
#endif

static constexpr uint16_t TMP102_CONFIGURATION_MASK = 0x1FD0;
static constexpr uint16_t TMP102_RESET_CONFIGURATION = 0x60A0;
static constexpr uint16_t TMP102_ONE_SHOT_BIT = 0x8000;
static constexpr uint16_t TMP102_SHUTDOWN_BIT = 0x0100;
static constexpr uint16_t TMP102_THERMOSTAT_MODE_BIT = 0x0200;
static constexpr uint16_t TMP102_EXTENDED_MODE_BIT = 0x0010;
static constexpr uint32_t TMP102_ONESHOT_DELAY_MS = 40;

static float decode_temperature(uint16_t raw, bool extended) {
  return (static_cast<int16_t>(raw) >> (extended ? 3 : 4)) * TMP102_CONVERSION_FACTOR;
}

#ifdef USE_TMP102_NUMBER
static uint16_t encode_temperature(float temperature, bool extended) {
  const int16_t steps = static_cast<int16_t>(std::lround(temperature / TMP102_CONVERSION_FACTOR));
  return static_cast<uint16_t>(static_cast<uint32_t>(static_cast<uint16_t>(steps)) << (extended ? 3 : 4));
}
#endif
#endif

#if defined(USE_TMP102_CONFIGURE) || defined(USE_TMP102_TEXT_SENSOR)
void TMP102Component::setup() {
#ifdef USE_TMP102_CONFIGURE
  if (this->configure_) {
    this->setup_configured_();
    return;
  }
#endif
  this->publish_threshold_status_(TMP102_THRESHOLD_STATUS_NOT_CONFIGURED);
}
#endif

#ifdef USE_TMP102_CONFIGURE
void TMP102Component::setup_configured_() {
  bool corrected = false;
#ifdef USE_TMP102_NUMBER
  float restored[2]{};
  bool restore_corrected[2]{};
  corrected = this->prepare_number_limits_(restored, restore_corrected);
#endif
  if (!this->write_configuration_()) {
    this->publish_threshold_status_(TMP102_THRESHOLD_STATUS_SETUP_FAILED);
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }
#ifdef USE_TMP102_NUMBER
  bool saved = true;
  for (uint8_t limit = 0; limit < 2; ++limit) {
    TMP102LimitNumber *control = this->limit_controls_[limit];
    if (control == nullptr)
      continue;
    const float value = this->get_limit_temperature_(static_cast<TMP102LimitType>(limit));
    if (restore_corrected[limit] || value != restored[limit])
      saved &= control->save_value_(value);
    control->publish_state(value);
  }
  if (!saved) {
    ESP_LOGW(TAG, "Failed to save threshold preferences");
    this->publish_threshold_status_(TMP102_THRESHOLD_STATUS_RESTORE_RESET_SAVE_FAILED);
    return;
  }
#endif
  this->publish_threshold_status_(corrected ? TMP102_THRESHOLD_STATUS_RESTORE_RESET : TMP102_THRESHOLD_STATUS_OK);
}
#endif

void TMP102Component::dump_config() {
  ESP_LOGCONFIG(TAG, "TMP102:");
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Temperature", this);
#ifdef USE_TMP102_BINARY_SENSOR
  LOG_BINARY_SENSOR("  ", "Alert", this->alert_binary_sensor_);
#endif
#ifdef USE_TMP102_TEXT_SENSOR
  LOG_TEXT_SENSOR("  ", "Threshold Status", this->threshold_status_text_sensor_);
#endif
#ifdef USE_TMP102_CONFIGURE
  if (!this->configure_)
    return;
  const uint8_t rate = (this->config_ >> 6) & 0x03;
  const LogString *rate_str = rate == 0   ? LOG_STR("0.25Hz")
                              : rate == 1 ? LOG_STR("1Hz")
                              : rate == 2 ? LOG_STR("4Hz")
                                          : LOG_STR("8Hz");
  const uint8_t fault_bits = (this->config_ >> 11) & 0x03;
  const bool extended = this->config_ & TMP102_EXTENDED_MODE_BIT;
  ESP_LOGCONFIG(
      TAG,
      "  Extended Mode: %s\n"
      "  One-Shot Mode: %s\n"
      "  Conversion Rate: %s\n"
      "  Thermostat Mode: %s\n"
      "  Alert Polarity: %s\n"
      "  Fault Queue: %u\n"
      "  Temperature High: %.4f°C\n"
      "  Temperature Low: %.4f°C",
      YESNO(extended), YESNO(this->config_ & TMP102_SHUTDOWN_BIT), LOG_STR_ARG(rate_str),
      this->config_ & TMP102_THERMOSTAT_MODE_BIT ? LOG_STR_LITERAL("Interrupt") : LOG_STR_LITERAL("Comparator"),
      this->config_ & TMP102_ALERT_POLARITY_BIT ? LOG_STR_LITERAL("Active High") : LOG_STR_LITERAL("Active Low"),
      fault_bits == 3 ? 6 : 1U << fault_bits, decode_temperature(this->high_limit_, extended),
      decode_temperature(this->low_limit_, extended));
#ifdef USE_TMP102_NUMBER
  LOG_NUMBER("  ", "High Limit", this->limit_controls_[TMP102_LIMIT_HIGH]);
  LOG_NUMBER("  ", "Low Limit", this->limit_controls_[TMP102_LIMIT_LOW]);
#endif
  if (this->config_ & TMP102_THERMOSTAT_MODE_BIT) {
    ESP_LOGW(TAG, "Register reads clear the interrupt-mode ALERT pin");
  }
#endif
}

void TMP102Component::update() {
#ifdef USE_TMP102_CONFIGURE
  if (this->configure_) {
    this->update_configured_();
    return;
  }
#endif
  if (this->write(&TMP102_REGISTER_TEMPERATURE, 1) != i2c::ERROR_OK) {
    this->set_warning_and_invalidate_alert_();
    return;
  }
  this->set_timeout(READ_TEMP_TIMEOUT_ID, 50, [this]() {
    int16_t raw_temperature;
    if (this->read(reinterpret_cast<uint8_t *>(&raw_temperature), 2) != i2c::ERROR_OK) {
      this->set_warning_and_invalidate_alert_();
      return;
    }
    raw_temperature = i2c::i2ctohs(raw_temperature);
    raw_temperature = raw_temperature >> 4;
    float temperature = raw_temperature * TMP102_CONVERSION_FACTOR;
    ESP_LOGD(TAG, "Got Temperature=%.1f°C", temperature);

    this->publish_state(temperature);
#ifdef USE_TMP102_BINARY_SENSOR
    if (this->alert_binary_sensor_ != nullptr && !this->read_alert_state_())
      return;
#endif
    this->status_clear_warning();
  });
}

#ifdef USE_TMP102_CONFIGURE
void TMP102Component::update_configured_() {
  if (!this->check_configuration_())
    return;
  if (this->config_ & TMP102_SHUTDOWN_BIT) {
    if (!this->write_register_(TMP102_REGISTER_CONFIGURATION, this->config_ | TMP102_ONE_SHOT_BIT)) {
      this->set_warning_and_invalidate_alert_();
      return;
    }
    this->set_timeout(READ_TEMP_TIMEOUT_ID, TMP102_ONESHOT_DELAY_MS, [this]() { this->read_temperature_(); });
  } else {
    this->read_temperature_();
  }
}

bool TMP102Component::write_register_(uint8_t reg, uint16_t value) {
  uint8_t frame[3] = {reg, static_cast<uint8_t>(value >> 8), static_cast<uint8_t>(value)};
  if (this->write(frame, 3) == i2c::ERROR_OK)
    return true;
  ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
  return false;
}

bool TMP102Component::write_configuration_() {
  return this->write_register_(TMP102_REGISTER_CONFIGURATION, this->config_) &&
         this->write_register_(TMP102_REGISTER_HIGH_LIMIT, this->high_limit_) &&
         this->write_register_(TMP102_REGISTER_LOW_LIMIT, this->low_limit_);
}

bool TMP102Component::check_configuration_() {
  uint16_t config;
  if (!this->read_byte_16(TMP102_REGISTER_CONFIGURATION, &config)) {
    this->set_warning_and_invalidate_alert_();
    return false;
  }
  bool matches = (config & TMP102_CONFIGURATION_MASK) == (this->config_ & TMP102_CONFIGURATION_MASK);
  // A chip reset is invisible in the configuration register when it matches the reset default; check the limits.
  if (matches &&
      (this->config_ & TMP102_CONFIGURATION_MASK) == (TMP102_RESET_CONFIGURATION & TMP102_CONFIGURATION_MASK)) {
    uint16_t high, low;
    if (!this->read_byte_16(TMP102_REGISTER_HIGH_LIMIT, &high) ||
        !this->read_byte_16(TMP102_REGISTER_LOW_LIMIT, &low)) {
      this->set_warning_and_invalidate_alert_();
      return false;
    }
    matches = high == this->high_limit_ && low == this->low_limit_;
  }
  if (matches)
    return true;

  ESP_LOGW(TAG, "Configuration changed; restoring configured settings and thresholds");
  this->set_warning_and_invalidate_alert_();
  if (!this->write_configuration_()) {
    ESP_LOGW(TAG, "Failed to restore configuration and thresholds");
    this->publish_threshold_status_(TMP102_THRESHOLD_STATUS_RECOVERY_FAILED);
  } else {
    this->publish_threshold_status_(TMP102_THRESHOLD_STATUS_RECOVERED);
  }
  return false;
}

void TMP102Component::read_temperature_() {
  uint16_t raw;
  if (!this->read_byte_16(TMP102_REGISTER_TEMPERATURE, &raw)) {
    this->set_warning_and_invalidate_alert_();
    return;
  }
  const float temperature = decode_temperature(raw, raw & 0x0001);
  ESP_LOGD(TAG, "Got Temperature=%.2f°C", temperature);
  this->publish_state(temperature);
#ifdef USE_TMP102_BINARY_SENSOR
  if (this->alert_binary_sensor_ != nullptr && !this->read_alert_state_())
    return;
#endif
  this->status_clear_warning();
}

#ifdef USE_TMP102_NUMBER
void TMP102Component::set_limit_control(TMP102LimitNumber *number) {
  this->limit_controls_[number->limit_type_] = number;
}

bool TMP102Component::prepare_number_limits_(float *restored, bool *restore_corrected) {
  const bool extended = this->config_ & TMP102_EXTENDED_MODE_BIT;
  uint16_t raw[2] = {this->low_limit_, this->high_limit_};
  bool corrected = false;
  for (uint8_t limit = 0; limit < 2; ++limit) {
    TMP102LimitNumber *control = this->limit_controls_[limit];
    if (control == nullptr)
      continue;
    restored[limit] = control->load_value_(decode_temperature(raw[limit], extended), restore_corrected[limit]);
    corrected |= restore_corrected[limit];
    raw[limit] = encode_temperature(restored[limit], extended);
  }
  if (static_cast<int16_t>(raw[TMP102_LIMIT_LOW]) > static_cast<int16_t>(raw[TMP102_LIMIT_HIGH])) {
    ESP_LOGW(TAG, "Restored thresholds are inverted; using configured values");
    return true;
  }
  this->low_limit_ = raw[TMP102_LIMIT_LOW];
  this->high_limit_ = raw[TMP102_LIMIT_HIGH];
  return corrected;
}

float TMP102Component::get_limit_temperature_(TMP102LimitType limit) const {
  const uint16_t raw = limit == TMP102_LIMIT_HIGH ? this->high_limit_ : this->low_limit_;
  return decode_temperature(raw, this->config_ & TMP102_EXTENDED_MODE_BIT);
}

bool TMP102Component::set_limit_temperature_(TMP102LimitType limit, float temperature) {
  const bool extended = this->config_ & TMP102_EXTENDED_MODE_BIT;
  const uint16_t raw = encode_temperature(temperature, extended);
  const bool high = limit == TMP102_LIMIT_HIGH;
  const uint16_t other = high ? this->low_limit_ : this->high_limit_;
  if (high ? static_cast<int16_t>(raw) < static_cast<int16_t>(other)
           : static_cast<int16_t>(raw) > static_cast<int16_t>(other)) {
    ESP_LOGW(TAG, "Rejected %s threshold %.4f°C: crosses %s threshold %.4f°C",
             high ? LOG_STR_LITERAL("high") : LOG_STR_LITERAL("low"), decode_temperature(raw, extended),
             high ? LOG_STR_LITERAL("low") : LOG_STR_LITERAL("high"), decode_temperature(other, extended));
    this->publish_threshold_rejection_(high, decode_temperature(raw, extended), decode_temperature(other, extended));
    return false;
  }
  if (!this->write_register_(static_cast<uint8_t>(TMP102_REGISTER_LOW_LIMIT + limit), raw)) {
    this->status_set_warning();
    this->publish_threshold_status_(high ? TMP102_THRESHOLD_STATUS_WRITE_HIGH_FAILED
                                         : TMP102_THRESHOLD_STATUS_WRITE_LOW_FAILED);
    return false;
  }
  (high ? this->high_limit_ : this->low_limit_) = raw;
  return true;
}
#endif
#endif

#ifdef USE_TMP102_TEXT_SENSOR
void TMP102Component::publish_threshold_status_(uint8_t status) {
  if (this->threshold_status_text_sensor_ == nullptr)
    return;
  char text[40];
  ESPHOME_strncpy_P(
      text,
      reinterpret_cast<ESPHOME_PGM_P>(ThresholdStatusStrings::get_progmem_str(status, TMP102_THRESHOLD_STATUS_OK)),
      sizeof(text));
  text[sizeof(text) - 1] = '\0';
  this->threshold_status_text_sensor_->publish_state(text);
}

void TMP102Component::publish_threshold_rejection_(bool high, float value, float other) {
  if (this->threshold_status_text_sensor_ == nullptr)
    return;
  char text[96];
  if (high) {
    ESPHOME_snprintf_P(text, sizeof(text), ESPHOME_PSTR("Rejected: high %.4f°C crosses low %.4f°C"), value, other);
  } else {
    ESPHOME_snprintf_P(text, sizeof(text), ESPHOME_PSTR("Rejected: low %.4f°C crosses high %.4f°C"), value, other);
  }
  this->threshold_status_text_sensor_->publish_state(text);
}
#endif

#ifdef USE_TMP102_BINARY_SENSOR
void TMP102Component::invalidate_alert_() {
  if (this->alert_binary_sensor_ != nullptr)
    this->alert_binary_sensor_->invalidate_state();
}

bool TMP102Component::read_alert_state_() {
  uint16_t config;
  if (!this->read_byte_16(TMP102_REGISTER_CONFIGURATION, &config)) {
    this->set_warning_and_invalidate_alert_();
    ESP_LOGW(TAG, "Failed to read alert state");
    return false;
  }
  // AL equals POL while the comparator status is active.
  const bool alert = bool(config & TMP102_ALERT_BIT) == bool(config & TMP102_ALERT_POLARITY_BIT);
  this->alert_binary_sensor_->publish_state(alert);
  return true;
}
#endif

#ifdef USE_TMP102_NUMBER
float TMP102LimitNumber::load_value_(float fallback, bool &corrected) {
  float value = fallback;
  if (this->restore_value_) {
    this->pref_ = this->make_entity_preference<float>();
    this->pref_.load(&value);
  }
  if (!std::isfinite(value) || value < this->traits.get_min_value() || value > this->traits.get_max_value()) {
    ESP_LOGW(TAG, "Invalid restored threshold; using configured value");
    value = fallback;
    corrected = true;
  }
  return value;
}

bool TMP102LimitNumber::save_value_(float value) { return !this->restore_value_ || this->pref_.save(&value); }

void TMP102LimitNumber::control(float value) {
  // A chip that failed setup holds nothing the entity could show
  if (this->parent_->is_failed()) {
    ESP_LOGW(TAG, "Rejected threshold change: sensor setup failed");
    this->parent_->publish_threshold_status_(TMP102_THRESHOLD_STATUS_SENSOR_SETUP_FAILED);
    return;
  }
  const bool accepted = this->parent_->set_limit_temperature_(this->limit_type_, value);
  value = this->parent_->get_limit_temperature_(this->limit_type_);
  this->publish_state(value);
  if (accepted && !this->save_value_(value)) {
    ESP_LOGW(TAG, "Failed to save threshold preference");
    this->parent_->publish_threshold_status_(TMP102_THRESHOLD_STATUS_PREFERENCE_SAVE_FAILED);
  } else if (accepted) {
    this->parent_->publish_threshold_status_(TMP102_THRESHOLD_STATUS_OK);
  }
}
#endif

}  // namespace esphome::tmp102
