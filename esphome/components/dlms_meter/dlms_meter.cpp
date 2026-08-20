// Ignore Zephyr. It doesn't have any encryption library.
#if defined(USE_ESP32) || defined(USE_ARDUINO) || defined(USE_HOST)

#include "dlms_meter.h"
#include "esphome/core/log.h"

#include <array>
#include <cstdio>

namespace esphome::dlms_meter {

static const char *const TAG = "dlms_meter";
static void log_callback(dlms_parser::LogLevel level, const char *fmt, va_list args) {
  std::array<char, 256> buf;
  vsnprintf(buf.data(), buf.size(), fmt, args);
  switch (level) {
    case dlms_parser::LogLevel::ERROR:
      ESP_LOGE(TAG, "%s", buf.data());
      break;
    case dlms_parser::LogLevel::WARNING:
      ESP_LOGW(TAG, "%s", buf.data());
      break;
    case dlms_parser::LogLevel::INFO:
      ESP_LOGI(TAG, "%s", buf.data());
      break;
    case dlms_parser::LogLevel::VERBOSE:
      ESP_LOGV(TAG, "%s", buf.data());
      break;
    case dlms_parser::LogLevel::VERY_VERBOSE:
      ESP_LOGVV(TAG, "%s", buf.data());
      break;
    case dlms_parser::LogLevel::DEBUG:
      ESP_LOGD(TAG, "%s", buf.data());
      break;
  }
}

DlmsMeterComponent::DlmsMeterComponent(const uint32_t receive_timeout_ms, const bool skip_crc_check,
                                       const char *decryption_key, const char *authentication_key,
                                       std::vector<CustomPattern> custom_patterns)
    : receive_timeout_ms_(receive_timeout_ms),
      skip_crc_check_(skip_crc_check),
      custom_patterns_(std::move(custom_patterns)),
      parser_([this](const dlms_parser::AxdrCapture &capture) { this->on_data_(capture); }, &this->decryptor_) {
  dlms_parser::Logger::set_log_function(log_callback);

  if (decryption_key != nullptr) {
    const auto key = dlms_parser::Aes128GcmDecryptionKey::from_hex(decryption_key);
    if (key.has_value()) {
      this->parser_.set_decryption_key(*key);
    } else {
      ESP_LOGE(TAG, "Failed to set decryption key: invalid key format");
    }
  }

  if (authentication_key != nullptr) {
    const auto key = dlms_parser::Aes128GcmAuthenticationKey::from_hex(authentication_key);
    if (key.has_value()) {
      this->parser_.set_authentication_key(*key);
    } else {
      ESP_LOGE(TAG, "Failed to set authentication key: invalid key format");
    }
  }

  this->parser_.set_skip_crc_check(this->skip_crc_check_);

  this->parser_.load_default_patterns();

  for (const auto &custom_pattern : this->custom_patterns_) {
    this->parser_.register_pattern(custom_pattern.name, custom_pattern.pattern, custom_pattern.priority,
                                   custom_pattern.default_obis);
  }
}

void DlmsMeterComponent::setup() { this->flush_rx_buffer_(); }

void DlmsMeterComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "DLMS Meter:");
  ESP_LOGCONFIG(TAG, "  Receive Timeout: %u ms", this->receive_timeout_ms_);
  ESP_LOGCONFIG(TAG, "  Skip CRC Check: %s", YESNO(this->skip_crc_check_));

  std::array<char, 24> obis_buf;

  for (const auto &custom_pattern : this->custom_patterns_) {
    const auto default_obis = custom_pattern.default_obis.to_string(obis_buf);
    ESP_LOGCONFIG(TAG, "  Custom Pattern: '%s' (name: %s, priority: %d, default_obis: %.*s)", custom_pattern.pattern,
                  custom_pattern.name, custom_pattern.priority, static_cast<int>(default_obis.size()),
                  default_obis.data());
  }

#ifdef USE_SENSOR
  for (const auto &[obis_id, sensor] : this->sensors_) {
    LOG_SENSOR("  ", "Numeric Sensor (OBIS)", sensor);
    const auto obis = obis_id.to_string(obis_buf);
    ESP_LOGCONFIG(TAG, "    OBIS: %.*s", static_cast<int>(obis.size()), obis.data());
  }
#endif

#ifdef USE_TEXT_SENSOR
  for (const auto &[obis_id, sensor] : this->text_sensors_) {
    LOG_TEXT_SENSOR("  ", "Text Sensor (OBIS)", sensor);
    const auto obis = obis_id.to_string(obis_buf);
    ESP_LOGCONFIG(TAG, "    OBIS: %.*s", static_cast<int>(obis.size()), obis.data());
  }
#endif

#ifdef USE_BINARY_SENSOR
  for (const auto &[obis_id, sensor] : this->binary_sensors_) {
    LOG_BINARY_SENSOR("  ", "Binary Sensor (OBIS)", sensor);
    const auto obis = obis_id.to_string(obis_buf);
    ESP_LOGCONFIG(TAG, "    OBIS: %.*s", static_cast<int>(obis.size()), obis.data());
  }
#endif
}

void DlmsMeterComponent::loop() {
  this->read_rx_buffer_();
  if (this->bytes_accumulated_ > 0 &&
      App.get_loop_component_start_time() - this->last_rx_char_time_ > this->receive_timeout_ms_) {
    this->process_frame_();
  }
}

void DlmsMeterComponent::flush_rx_buffer_() {
  while (this->available()) {
    this->read();
  }
}

void DlmsMeterComponent::read_rx_buffer_() {
  int available = this->available();
  if (available == 0)
    return;

  if (this->bytes_accumulated_ + available > this->rx_buffer_.size()) {
    ESP_LOGW(TAG, "RX Buffer overflow. Frame too large! Dropping frame.");
    this->bytes_accumulated_ = 0;

    this->flush_rx_buffer_();
    return;
  }

  bool success = this->read_array(this->rx_buffer_.data() + this->bytes_accumulated_, available);
  if (!success) {
    ESP_LOGW(TAG, "UART read failed. Dropping frame.");
    this->bytes_accumulated_ = 0;
    this->flush_rx_buffer_();
    return;
  }

  this->bytes_accumulated_ += available;

  this->last_rx_char_time_ = App.get_loop_component_start_time();
}

void DlmsMeterComponent::process_frame_() {
  ESP_LOGV(TAG, "Processing frame of size: %zu bytes", this->bytes_accumulated_);

  this->parser_.parse({this->rx_buffer_.data(), this->bytes_accumulated_});

  this->bytes_accumulated_ = 0;
}

void DlmsMeterComponent::on_data_(const dlms_parser::AxdrCapture &capture) {
  int updated_count = 0;

#ifdef USE_SENSOR
  if (capture.is_numeric()) {
    if (const auto it = this->sensors_.find(capture.obis); it != this->sensors_.end()) {
      it->second->publish_state(capture.value_as_float_with_scaler_applied());
      updated_count++;
    }
  }
#endif

#ifdef USE_TEXT_SENSOR
  if (!capture.is_numeric()) {
    if (const auto it = this->text_sensors_.find(capture.obis); it != this->text_sensors_.end()) {
      std::array<char, 128> value_buf;
      const auto value = capture.value_as_string(value_buf);
      it->second->publish_state(value.data(), value.size());
      updated_count++;
    }
  }
#endif

#ifdef USE_BINARY_SENSOR
  if (capture.is_numeric()) {
    const bool state = capture.value_as_float_with_scaler_applied() != 0.0f;
    if (const auto it = this->binary_sensors_.find(capture.obis); it != this->binary_sensors_.end()) {
      it->second->publish_state(state);
      updated_count++;
    }
  }
#endif

  if (updated_count == 0) {
    std::array<char, 24> obis_buf;
    const auto obis = capture.obis.to_string(obis_buf);
    ESP_LOGV(TAG, "Received OBIS %.*s, but no sensors are registered for it.", static_cast<int>(obis.size()),
             obis.data());
  }
}

#ifdef USE_SENSOR
void DlmsMeterComponent::register_sensor(const dlms_parser::ObisId &obis, sensor::Sensor *sensor) {
  this->sensors_[obis] = sensor;
}
#endif

#ifdef USE_TEXT_SENSOR
void DlmsMeterComponent::register_text_sensor(const dlms_parser::ObisId &obis, text_sensor::TextSensor *sensor) {
  this->text_sensors_[obis] = sensor;
}
#endif

#ifdef USE_BINARY_SENSOR
void DlmsMeterComponent::register_binary_sensor(const dlms_parser::ObisId &obis, binary_sensor::BinarySensor *sensor) {
  this->binary_sensors_[obis] = sensor;
}
#endif

}  // namespace esphome::dlms_meter

#endif
