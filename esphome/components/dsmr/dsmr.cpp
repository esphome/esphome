#ifdef USE_ARDUINO

#include "dsmr.h"
#include "esphome/core/log.h"

#include <AES.h>
#include <Crypto.h>
#include <GCM.h>

namespace esphome {
namespace dsmr {

static const char *const TAG = "dsmr";

void Dsmr::setup() {
  this->telegram_ = new char[this->max_telegram_len_];  // NOLINT
  if (this->request_pin_ != nullptr) {
    this->request_pin_->setup();
  }
}

void Dsmr::loop() {
  if (this->ready_to_request_data_()) {
    if (this->decryption_key_.empty()) {
      this->receive_telegram_();
    } else {
      this->receive_encrypted_();
    }
  }
}

bool Dsmr::ready_to_request_data_() {
  // When using a request pin, then wait for the next request interval.
  if (this->request_pin_ != nullptr) {
    if (!this->requesting_data_ && this->request_interval_reached_()) {
      this->start_requesting_data_();
    }
  }
  // Otherwise, sink serial data until next request interval.
  else {
    if (this->request_interval_reached_()) {
      this->start_requesting_data_();
    }
    if (!this->requesting_data_) {
      while (this->available()) {
        this->read();
      }
    }
  }
  return this->requesting_data_;
}

bool Dsmr::request_interval_reached_() {
  if (this->last_request_time_ == 0) {
    return true;
  }
  return millis() - this->last_request_time_ > this->request_interval_;
}

bool Dsmr::available_within_timeout_() {
  uint8_t tries = READ_TIMEOUT_MS / 5;
  while (tries--) {
    delay(5);
    if (this->available()) {
      return true;
    }
  }
  return false;
}

void Dsmr::start_requesting_data_() {
  if (!this->requesting_data_) {
    if (this->request_pin_ != nullptr) {
      ESP_LOGV(TAG, "Start requesting data from P1 port");
      this->request_pin_->digital_write(true);
    } else {
      ESP_LOGV(TAG, "Start reading data from P1 port");
    }
    this->requesting_data_ = true;
    this->last_request_time_ = millis();
  }
}

void Dsmr::stop_requesting_data_() {
  if (this->requesting_data_) {
    if (this->request_pin_ != nullptr) {
      ESP_LOGV(TAG, "Stop requesting data from P1 port");
      this->request_pin_->digital_write(false);
    } else {
      ESP_LOGV(TAG, "Stop reading data from P1 port");
    }
    while (this->available()) {
      this->read();
    }
    this->requesting_data_ = false;
  }
}

void Dsmr::receive_telegram_() {
  while (true) {
    if (!this->available()) {
      if (!this->header_found_ || !this->available_within_timeout_()) {
        return;
      }
    }

    const char c = this->read();

    // Find a new telegram header, i.e. forward slash.
    if (c == '/') {
      ESP_LOGV(TAG, "Header of telegram found");
      this->header_found_ = true;
      this->footer_found_ = false;
      this->telegram_len_ = 0;
    }
    if (!this->header_found_)
      continue;

    // Check for buffer overflow.
    if (this->telegram_len_ >= this->max_telegram_len_) {
      this->header_found_ = false;
      this->footer_found_ = false;
      ESP_LOGE(TAG, "Error: telegram larger than buffer (%d bytes)", this->max_telegram_len_);
      return;
    }

    // Some v2.2 or v3 meters will send a new value which starts with '('
    // in a new line, while the value belongs to the previous ObisId. For
    // proper parsing, remove these new line characters.
    if (c == '(') {
      while (true) {
        auto previous_char = this->telegram_[this->telegram_len_ - 1];
        if (previous_char == '\n' || previous_char == '\r') {
          this->telegram_len_--;
        } else {
          break;
        }
      }
    }

    // Store the byte in the buffer.
    this->telegram_[this->telegram_len_] = c;
    this->telegram_len_++;

    // Check for a footer, i.e. exlamation mark, followed by a hex checksum.
    if (c == '!') {
      ESP_LOGV(TAG, "Footer of telegram found");
      this->footer_found_ = true;
      continue;
    }
    // Check for the end of the hex checksum, i.e. a newline.
    if (this->footer_found_ && c == '\n') {
      // Parse the telegram and publish sensor values.
      this->parse_telegram();

      this->header_found_ = false;
      return;
    }
  }
}

void Dsmr::receive_encrypted_() {
  this->encrypted_telegram_len_ = 0;
  size_t packet_size = 0;

  while (true) {
    if (!this->available()) {
      if (!this->header_found_) {
        return;
      }
      if (!this->available_within_timeout_()) {
        ESP_LOGW(TAG, "Timeout while reading data for encrypted telegram");
        return;
      }
    }

    const char c = this->read();

    // Find a new telegram start byte.
    if (!this->header_found_) {
      if ((uint8_t) c != 0xDB) {
        continue;
      }
      ESP_LOGV(TAG, "Start byte 0xDB of encrypted telegram found");
      this->header_found_ = true;
    }

    // Check for buffer overflow.
    if (this->encrypted_telegram_len_ >= this->max_telegram_len_) {
      this->header_found_ = false;
      ESP_LOGE(TAG, "Error: encrypted telegram larger than buffer (%d bytes)", this->max_telegram_len_);
      return;
    }

    this->encrypted_telegram_[this->encrypted_telegram_len_++] = c;

    if (packet_size == 0 && this->encrypted_telegram_len_ > 20) {
      // Complete header + data bytes
      packet_size = 13 + (this->encrypted_telegram_[11] << 8 | this->encrypted_telegram_[12]);
      ESP_LOGV(TAG, "Encrypted telegram size: %d bytes", packet_size);
    }
    if (this->encrypted_telegram_len_ == packet_size && packet_size > 0) {
      ESP_LOGV(TAG, "End of encrypted telegram found");
      GCM<AES128> *gcmaes128{new GCM<AES128>()};
      gcmaes128->setKey(this->decryption_key_.data(), gcmaes128->keySize());
      // the iv is 8 bytes of the system title + 4 bytes frame counter
      // system title is at byte 2 and frame counter at byte 15
      for (int i = 10; i < 14; i++)
        this->encrypted_telegram_[i] = this->encrypted_telegram_[i + 4];
      constexpr uint16_t iv_size{12};
      gcmaes128->setIV(&this->encrypted_telegram_[2], iv_size);
      gcmaes128->decrypt(reinterpret_cast<uint8_t *>(this->telegram_),
                         // the ciphertext start at byte 18
                         &this->encrypted_telegram_[18],
                         // cipher size
                         this->encrypted_telegram_len_ - 17);
      delete gcmaes128;  // NOLINT(cppcoreguidelines-owning-memory)

      this->telegram_len_ = strnlen(this->telegram_, this->max_telegram_len_);
      ESP_LOGV(TAG, "Decrypted telegram size: %d bytes", this->telegram_len_);
      ESP_LOGVV(TAG, "Decrypted telegram: %s", this->telegram_);

      this->parse_telegram();

      this->header_found_ = false;
      this->telegram_len_ = 0;
      return;
    }
  }
}

bool Dsmr::parse_telegram() {
  MyData data;
  ESP_LOGV(TAG, "Trying to parse telegram");
  this->stop_requesting_data_();
  ::dsmr::ParseResult<void> res =
      ::dsmr::P1Parser::parse(&data, this->telegram_, this->telegram_len_, false,
                              this->crc_check_);  // Parse telegram according to data definition. Ignore unknown values.
  if (res.err) {
    // Parsing error, show it
    auto err_str = res.fullError(this->telegram_, this->telegram_ + this->telegram_len_);
    ESP_LOGE(TAG, "%s", err_str.c_str());
    return false;
  } else {
    this->status_clear_warning();
    this->publish_sensors(data);
    return true;
  }
}

void Dsmr::dump_config() {
  ESP_LOGCONFIG(TAG, "DSMR:");
  ESP_LOGCONFIG(TAG, "  Max telegram length: %d", this->max_telegram_len_);

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

void Dsmr::set_decryption_key(const std::string &decryption_key) {
  if (decryption_key.length() == 0) {
    ESP_LOGI(TAG, "Disabling decryption");
    this->decryption_key_.clear();
    if (this->encrypted_telegram_ != nullptr) {
      delete[] this->encrypted_telegram_;
      this->encrypted_telegram_ = nullptr;
    }
    return;
  }

  if (decryption_key.length() != 32) {
    ESP_LOGE(TAG, "Error, decryption key must be 32 character long");
    return;
  }
  this->decryption_key_.clear();

  ESP_LOGI(TAG, "Decryption key is set");
  // Verbose level prints decryption key
  ESP_LOGV(TAG, "Using decryption key: %s", decryption_key.c_str());

  char temp[3] = {0};
  for (int i = 0; i < 16; i++) {
    strncpy(temp, &(decryption_key.c_str()[i * 2]), 2);
    this->decryption_key_.push_back(std::strtoul(temp, nullptr, 16));
  }

  if (this->encrypted_telegram_ == nullptr) {
    this->encrypted_telegram_ = new uint8_t[this->max_telegram_len_];  // NOLINT
  }
}

void Dsmr::set_max_telegram_length(size_t length) { max_telegram_len_ = length; }

}  // namespace dsmr
}  // namespace esphome

#endif  // USE_ARDUINO
