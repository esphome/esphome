// Ignore Zephyr. It doesn't have any encryption library.
#if defined(USE_ESP32) || defined(USE_ARDUINO) || defined(USE_HOST)

#include "dsmr.h"
#include "esphome/core/log.h"
#include <dsmr_parser/util.h>

namespace esphome::dsmr {

static constexpr auto &TAG = "dsmr";

static void log_callback(dsmr_parser::LogLevel level, const char *fmt, va_list args) {
  std::array<char, 256> buf;
  vsnprintf(buf.data(), buf.size(), fmt, args);
  switch (level) {
    case dsmr_parser::LogLevel::ERROR:
      ESP_LOGE(TAG, "%s", buf.data());
      break;
    case dsmr_parser::LogLevel::WARNING:
      ESP_LOGW(TAG, "%s", buf.data());
      break;
    case dsmr_parser::LogLevel::INFO:
      ESP_LOGI(TAG, "%s", buf.data());
      break;
    case dsmr_parser::LogLevel::VERBOSE:
      ESP_LOGV(TAG, "%s", buf.data());
      break;
    case dsmr_parser::LogLevel::VERY_VERBOSE:
      ESP_LOGVV(TAG, "%s", buf.data());
      break;
    case dsmr_parser::LogLevel::DEBUG:
      ESP_LOGD(TAG, "%s", buf.data());
      break;
  }
}

void Dsmr::setup() {
  dsmr_parser::Logger::set_log_function(log_callback);
  if (this->request_pin_ != nullptr) {
    this->request_pin_->setup();
  }
}

void Dsmr::loop() {
  if (!this->ready_to_request_data_()) {
    return;
  }

  if (this->encryption_enabled_) {
    this->receive_encrypted_telegram_();
  } else {
    this->receive_telegram_();
  }
}

bool Dsmr::ready_to_request_data_() {
  if (!this->requesting_data_ && this->request_interval_reached_()) {
    this->start_requesting_data_();
  }
  return this->requesting_data_;
}

bool Dsmr::request_interval_reached_() const {
  if (this->last_request_time_ == 0) {
    return true;
  }
  return millis() - this->last_request_time_ > this->request_interval_;
}

void Dsmr::start_requesting_data_() {
  if (this->requesting_data_) {
    return;
  }

  ESP_LOGV(TAG, "Start reading data from P1 port");
  this->flush_rx_buffer_();

  if (this->request_pin_ != nullptr) {
    ESP_LOGV(TAG, "Set request pin to 1");
    this->request_pin_->digital_write(true);
  }

  this->requesting_data_ = true;
  this->last_request_time_ = millis();
}

void Dsmr::stop_requesting_data_() {
  if (!this->requesting_data_) {
    return;
  }

  ESP_LOGV(TAG, "Stop reading data from P1 port");
  if (this->request_pin_ != nullptr) {
    ESP_LOGV(TAG, "Set request pin to 0");
    this->request_pin_->digital_write(false);
  }
  this->requesting_data_ = false;
}

void Dsmr::flush_rx_buffer_() {
  ESP_LOGV(TAG, "Flush UART RX buffer");
  while (!this->uart_read_chunk_().empty()) {
  }
}

void Dsmr::receive_telegram_() {
  for (auto data = this->uart_read_chunk_(); !data.empty(); data = this->uart_read_chunk_()) {
    for (uint8_t byte : data) {
      const auto telegram = this->packet_accumulator_.process_byte(byte);
      if (!telegram) {  // No full packet received yet
        continue;
      }
      if (this->parse_telegram_(telegram.value())) {
        return;
      }
    }
  }
}

void Dsmr::receive_encrypted_telegram_() {
  for (auto data = this->uart_read_chunk_(); !data.empty(); data = this->uart_read_chunk_()) {
    for (uint8_t byte : data) {
      if (this->buffer_pos_ >= this->buffer_.size()) {  // Reset buffer if overflow
        ESP_LOGW(TAG, "Encrypted buffer overflow, resetting");
        this->buffer_pos_ = 0;
      }

      this->buffer_[this->buffer_pos_] = byte;
      this->buffer_pos_++;
    }
    this->last_read_time_ = millis();
  }

  // Detect inter-frame delay. If no byte is received for more than receive_timeout, then the packet is complete.
  if (millis() - this->last_read_time_ > this->receive_timeout_ && this->buffer_pos_ > 0) {
    ESP_LOGV(TAG, "Encrypted telegram received (%zu bytes)", this->buffer_pos_);

    const auto telegram = this->dlms_decryptor_.decrypt_inplace({this->buffer_.data(), this->buffer_pos_});

    // Reset buffer position for the next packet
    this->buffer_pos_ = 0;
    this->last_read_time_ = 0;

    if (!telegram) {  // decryption failed
      return;
    }

    // Parse and publish the telegram
    this->parse_telegram_(telegram.value());
  }
}

bool Dsmr::parse_telegram_(const dsmr_parser::DsmrUnencryptedTelegram &telegram) {
  this->stop_requesting_data_();

  ESP_LOGV(TAG, "Trying to parse telegram (%zu bytes)", telegram.content().size());
  ESP_LOGVV(TAG, "Telegram content:\n %.*s", static_cast<int>(telegram.content().size()), telegram.content().data());

  MyData data;
  if (const bool res = dsmr_parser::DsmrParser::parse(data, telegram); !res) {
    ESP_LOGE(TAG, "Failed to parse telegram");
    return false;
  }

  this->status_clear_warning();
  this->publish_sensors(data);

  // Publish the telegram, after publishing the sensors so it can also trigger action based on latest values
  if (this->s_telegram_ != nullptr) {
    this->s_telegram_->publish_state(telegram.content().data(), telegram.content().size());
  }
  return true;
}

void Dsmr::dump_config() {
  ESP_LOGCONFIG(TAG,
                "DSMR:\n"
                "  Max telegram length: %zu\n"
                "  Receive timeout: %.1fs",
                this->buffer_.size(), this->receive_timeout_ / 1e3f);
  if (this->request_pin_ != nullptr) {
    LOG_PIN("  Request Pin: ", this->request_pin_);
  }
  if (this->request_interval_ > 0) {
    ESP_LOGCONFIG(TAG, "  Request Interval: %.1fs", this->request_interval_ / 1e3f);
  }

#define DSMR_LOG_SENSOR(s) LOG_SENSOR("  ", #s, this->s_##s##_);
  DSMR_SENSOR_LIST(DSMR_LOG_SENSOR, )

#define DSMR_LOG_TEXT_SENSOR(s) LOG_TEXT_SENSOR("  ", #s, this->s_##s##_);
  DSMR_TEXT_SENSOR_LIST(DSMR_LOG_TEXT_SENSOR, )
}

void Dsmr::set_decryption_key_(const char *decryption_key) {
  if (decryption_key == nullptr || decryption_key[0] == '\0') {
    this->encryption_enabled_ = false;
    return;
  }

  auto key = dsmr_parser::Aes128GcmDecryptionKey::from_hex(decryption_key);
  if (!key) {
    ESP_LOGE(TAG, "Error, decryption key has incorrect format");
    this->encryption_enabled_ = false;
    return;
  }

  ESP_LOGI(TAG, "Decryption key is set");

  this->gcm_decryptor_.set_encryption_key(key.value());
  this->encryption_enabled_ = true;
}

std::span<uint8_t> Dsmr::uart_read_chunk_() {
  const auto avail = this->available();
  if (avail == 0) {
    return {};
  }
  size_t to_read = std::min(avail, uart_chunk_reading_buf_.size());
  if (!this->read_array(uart_chunk_reading_buf_.data(), to_read)) {
    return {};
  }
  return {uart_chunk_reading_buf_.data(), to_read};
}

}  // namespace esphome::dsmr

#endif
