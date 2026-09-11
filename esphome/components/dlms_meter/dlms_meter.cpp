#include "dlms_meter.h"
#include "esphome/core/log.h"

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

DlmsMeterComponent::DlmsMeterComponent(uint32_t receive_timeout_ms, bool skip_crc_check,
                                       std::optional<std::array<uint8_t, 16>> decryption_key,
                                       std::optional<std::array<uint8_t, 16>> authentication_key,
                                       std::vector<CustomPattern> custom_patterns)
    : receive_timeout_ms_(receive_timeout_ms),
      skip_crc_check_(skip_crc_check),
      custom_patterns_(std::move(custom_patterns)),
      parser_(&decryptor_) {
  dlms_parser::Logger::set_log_function(log_callback);

  if (decryption_key.has_value()) {
#ifdef DLMS_METER_NO_CRYPTO
    ESP_LOGE(TAG, "Decryption is not supported on this platform (no compatible crypto library found)");
#else
    auto opt_key = dlms_parser::Aes128GcmDecryptionKey::from_bytes(decryption_key.value());
    if (opt_key) {
      this->parser_.set_decryption_key(*opt_key);
    } else {
      ESP_LOGE(TAG, "Failed to set decryption key: invalid key format");
    }
#endif
  }

  if (authentication_key.has_value()) {
#ifdef DLMS_METER_NO_CRYPTO
    ESP_LOGE(TAG, "Authentication is not supported on this platform (no compatible crypto library found)");
#else
    auto opt_key = dlms_parser::Aes128GcmAuthenticationKey::from_bytes(authentication_key.value());
    if (opt_key) {
      this->parser_.set_authentication_key(*opt_key);
    } else {
      ESP_LOGE(TAG, "Failed to set authentication key: invalid key format");
    }
#endif
  }

  this->parser_.set_skip_crc_check(this->skip_crc_check_);

  this->parser_.load_default_patterns();
  for (const auto &pattern : this->custom_patterns_) {
    if (pattern.default_obis.has_value() && pattern.name.has_value()) {
      this->parser_.register_pattern(pattern.name->c_str(), pattern.pattern.c_str(), pattern.priority,
                                     pattern.default_obis.value());
    } else if (pattern.name.has_value()) {
      this->parser_.register_pattern(pattern.name->c_str(), pattern.pattern.c_str(), pattern.priority);
    } else {
      this->parser_.register_pattern(pattern.pattern.c_str());
    }
  }
}

void DlmsMeterComponent::setup() { this->flush_rx_buffer_(); }

void DlmsMeterComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "DLMS Meter:");
  ESP_LOGCONFIG(TAG, "  Receive Timeout: %u ms", this->receive_timeout_ms_);
  ESP_LOGCONFIG(TAG, "  Skip CRC Check: %s", YESNO(this->skip_crc_check_));

  for (const auto &pattern : this->custom_patterns_) {
    if (pattern.default_obis.has_value() && pattern.name.has_value()) {
      const auto &obis = pattern.default_obis.value();
      ESP_LOGCONFIG(TAG, "  Custom Pattern: '%s' (name: %s, priority: %d, default_obis: %d.%d.%d.%d.%d.%d)",
                    pattern.pattern.c_str(), pattern.name->c_str(), pattern.priority, obis[0], obis[1], obis[2],
                    obis[3], obis[4], obis[5]);
    } else if (pattern.name.has_value()) {
      ESP_LOGCONFIG(TAG, "  Custom Pattern: '%s' (name: %s, priority: %d)", pattern.pattern.c_str(),
                    pattern.name->c_str(), pattern.priority);
    } else {
      ESP_LOGCONFIG(TAG, "  Custom Pattern: '%s'", pattern.pattern.c_str());
    }
  }

#ifdef USE_SENSOR
  for (const auto &entry : this->sensors_) {
    LOG_SENSOR("  ", "Numeric Sensor (OBIS)", entry.sensor);
    ESP_LOGCONFIG(TAG, "    OBIS: %s", entry.obis_code.c_str());
  }
#endif
#ifdef USE_TEXT_SENSOR
  for (const auto &entry : this->text_sensors_) {
    LOG_TEXT_SENSOR("  ", "Text Sensor (OBIS)", entry.sensor);
    ESP_LOGCONFIG(TAG, "    OBIS: %s", entry.obis_code.c_str());
  }
#endif
#ifdef USE_BINARY_SENSOR
  for (const auto &entry : this->binary_sensors_) {
    LOG_BINARY_SENSOR("  ", "Binary Sensor (OBIS)", entry.sensor);
    ESP_LOGCONFIG(TAG, "    OBIS: %s", entry.obis_code.c_str());
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

  auto callback = [this](const char *obis_code, float float_val, const char *str_val, bool is_numeric) {
    this->on_data_(obis_code, float_val, str_val, is_numeric);
  };

  this->parser_.parse({this->rx_buffer_.data(), this->bytes_accumulated_}, callback);

  this->bytes_accumulated_ = 0;
}

void DlmsMeterComponent::on_data_(const char *obis_code, float float_val, const char *str_val, bool is_numeric) {
  int updated_count = 0;

#ifdef USE_SENSOR
  if (is_numeric) {
    for (auto &item : this->sensors_) {
      if (item.obis_code == obis_code) {
        item.sensor->publish_state(float_val);
        updated_count++;
      }
    }
  }
#endif

#ifdef USE_TEXT_SENSOR
  if (!is_numeric && str_val != nullptr) {
    for (auto &item : this->text_sensors_) {
      if (item.obis_code == obis_code) {
        item.sensor->publish_state(str_val);
        updated_count++;
      }
    }
  }
#endif

#ifdef USE_BINARY_SENSOR
  if (is_numeric) {
    bool state = float_val != 0.0f;
    for (auto &item : this->binary_sensors_) {
      if (item.obis_code == obis_code) {
        item.sensor->publish_state(state);
        updated_count++;
      }
    }
  }
#endif

  if (updated_count == 0) {
    ESP_LOGV(TAG, "Received OBIS %s, but no sensors are registered for it.", obis_code);
  }
}

#ifdef USE_SENSOR
void DlmsMeterComponent::register_sensor(const std::string &obis_code, sensor::Sensor *sensor) {
  this->sensors_.push_back({obis_code, sensor});
}
#endif
#ifdef USE_TEXT_SENSOR
void DlmsMeterComponent::register_text_sensor(const std::string &obis_code, text_sensor::TextSensor *sensor) {
  this->text_sensors_.push_back({obis_code, sensor});
}
#endif
#ifdef USE_BINARY_SENSOR
void DlmsMeterComponent::register_binary_sensor(const std::string &obis_code, binary_sensor::BinarySensor *sensor) {
  this->binary_sensors_.push_back({obis_code, sensor});
}
#endif

}  // namespace esphome::dlms_meter
