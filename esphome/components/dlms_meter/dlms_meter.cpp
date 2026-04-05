#include "dlms_meter.h"
#include "esphome/core/log.h"

#include <cstdio>

namespace esphome::dlms_meter {

static constexpr const char *TAG = "dlms_meter";

static void log_callback(dlms_parser::LogLevel level, const char *fmt, va_list args) {
  static char buf[256];
  vsnprintf(buf, sizeof(buf), fmt, args);
  switch (level) {
    case dlms_parser::LogLevel::ERROR:
      ESP_LOGE(TAG, "%s", buf);
      break;
    case dlms_parser::LogLevel::WARNING:
      ESP_LOGW(TAG, "%s", buf);
      break;
    case dlms_parser::LogLevel::INFO:
      ESP_LOGI(TAG, "%s", buf);
      break;
    case dlms_parser::LogLevel::DEBUG:
      ESP_LOGD(TAG, "%s", buf);
      break;
    case dlms_parser::LogLevel::VERBOSE:
      ESP_LOGV(TAG, "%s", buf);
      break;
    default:
      ESP_LOGVV(TAG, "%s", buf);
      break;
  }
}

void DlmsMeterComponent::setup() {
  dlms_parser::Logger::set_log_function(log_callback);

  this->parser_.set_skip_crc_check(this->skip_crc_check_);
  this->parser_.load_default_patterns();
  for (const auto &pattern : this->custom_patterns_) {
    this->parser_.register_pattern(pattern.c_str());
  }

  while (this->available()) {
    this->read();
  }
}

void DlmsMeterComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "DLMS Meter:");
  ESP_LOGCONFIG(TAG, "  Receive Timeout: %u ms", this->receive_timeout_ms_);
  ESP_LOGCONFIG(TAG, "  Skip CRC Check: %s", YESNO(this->skip_crc_check_));

  for (const auto &pattern : this->custom_patterns_) {
    ESP_LOGCONFIG(TAG, "  Custom Pattern: %s", pattern.c_str());
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

void DlmsMeterComponent::set_decryption_key(const std::array<uint8_t, 16> &key) {
  auto opt_key = dlms_parser::Aes128GcmDecryptionKey::from_bytes(key);
  if (opt_key) {
    this->parser_.set_decryption_key(*opt_key);
  } else {
    ESP_LOGE(TAG, "Failed to set decryption key: invalid key format");
  }
}

void DlmsMeterComponent::set_authentication_key(const std::array<uint8_t, 16> &key) {
  auto opt_key = dlms_parser::Aes128GcmAuthenticationKey::from_bytes(key);
  if (opt_key) {
    this->parser_.set_authentication_key(*opt_key);
  } else {
    ESP_LOGE(TAG, "Failed to set authentication key: invalid key format");
  }
}

void DlmsMeterComponent::loop() {
  this->read_rx_buffer_();
  if (this->receiving_ && millis() - this->last_rx_char_time_ > this->receive_timeout_ms_) {
    this->process_frame_();
  }
}

void DlmsMeterComponent::read_rx_buffer_() {
  int available = this->available();
  if (available == 0)
    return;

  this->receiving_ = true;

  if (this->bytes_accumulated_ + available > this->rx_buffer_.size()) {
    ESP_LOGW(TAG, "RX Buffer overflow. Frame too large! Dropping frame.");
    this->bytes_accumulated_ = 0;
    this->receiving_ = false;

    while (this->available()) {
      this->read();
    }
    return;
  }

  this->read_array(this->rx_buffer_.data() + this->bytes_accumulated_, available);
  this->bytes_accumulated_ += available;

  // Updated after consuming the chars from the buffer
  this->last_rx_char_time_ = millis();
}

void DlmsMeterComponent::process_frame_() {
  if (this->bytes_accumulated_ == 0)
    return;

  ESP_LOGD(TAG, "Processing frame of size: %zu bytes", this->bytes_accumulated_);

  auto callback = [this](const char *obis_code, float float_val, const char *str_val, bool is_numeric) {
    this->on_data_(obis_code, float_val, str_val, is_numeric);
  };

  this->parser_.parse(std::span<uint8_t>{this->rx_buffer_.data(), this->bytes_accumulated_}, callback);

  this->bytes_accumulated_ = 0;
  this->receiving_ = false;
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
