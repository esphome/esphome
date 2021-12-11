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
      this->receive_encrypted_telegram_();
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

bool Dsmr::receive_timeout_reached_() { return millis() - this->last_read_time_ > this->receive_timeout_; }

bool Dsmr::available_within_timeout_() {
  // Data are available for reading on the UART bus?
  // Then we can start reading right away.
  if (this->available()) {
    this->last_read_time_ = millis();
    return true;
  }
  // When we're not in the process of reading a telegram, then there is
  // no need to actively wait for new data to come in.
  if (!header_found_) {
    return false;
  }
  // A telegram is being read. The smart meter might not deliver a telegram
  // in one go, but instead send it in chunks with small pauses in between.
  // When the UART RX buffer cannot hold a full telegram, then make sure
  // that the UART read buffer does not overflow while other components
  // perform their work in their loop. Do this by not returning control to
  // the main loop, until the read timeout is reached.
  if (this->parent_->get_rx_buffer_size() < this->max_telegram_len_) {
    while (!this->receive_timeout_reached_()) {
      delay(5);
      if (this->available()) {
        this->last_read_time_ = millis();
        return true;
      }
    }
  }
  // No new data has come in during the read timeout? Then stop reading the
  // telegram and start waiting for the next one to arrive.
  if (this->receive_timeout_reached_()) {
    ESP_LOGW(TAG, "Timeout while reading data for telegram");
    this->reset_telegram_();
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

void Dsmr::reset_telegram_() {
  this->header_found_ = false;
  this->footer_found_ = false;
  this->bytes_read_ = 0;
  this->crypt_bytes_read_ = 0;
  this->crypt_telegram_len_ = 0;
  this->last_read_time_ = 0;
}

void Dsmr::receive_telegram_() {
  while (this->available_within_timeout_()) {
    const char c = this->read();

    // Find a new telegram header, i.e. forward slash.
    if (c == '/') {
      ESP_LOGV(TAG, "Header of telegram found");
      this->reset_telegram_();
      this->header_found_ = true;
    }
    if (!this->header_found_)
      continue;

    // Check for buffer overflow.
    if (this->bytes_read_ >= this->max_telegram_len_) {
      this->reset_telegram_();
      ESP_LOGE(TAG, "Error: telegram larger than buffer (%d bytes)", this->max_telegram_len_);
      return;
    }

    // Some v2.2 or v3 meters will send a new value which starts with '('
    // in a new line, while the value belongs to the previous ObisId. For
    // proper parsing, remove these new line characters.
    if (c == '(') {
      while (true) {
        auto previous_char = this->telegram_[this->bytes_read_ - 1];
        if (previous_char == '\n' || previous_char == '\r') {
          this->bytes_read_--;
        } else {
          break;
        }
      }
    }

    // Store the byte in the buffer.
    this->telegram_[this->bytes_read_] = c;
    this->bytes_read_++;

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
      this->reset_telegram_();
      return;
    }
  }
}

void Dsmr::receive_encrypted_telegram_() {
  while (this->available_within_timeout_()) {
    const char c = this->read();

    // Find a new telegram start byte.
    if (!this->header_found_) {
      if ((uint8_t) c != 0xDB) {
        continue;
      }
      ESP_LOGV(TAG, "Start byte 0xDB of encrypted telegram found");
      this->reset_telegram_();
      this->header_found_ = true;
    }

    // Check for buffer overflow.
    if (this->crypt_bytes_read_ >= this->max_telegram_len_) {
      this->reset_telegram_();
      ESP_LOGE(TAG, "Error: encrypted telegram larger than buffer (%d bytes)", this->max_telegram_len_);
      return;
    }

    // Store the byte in the buffer.
    this->crypt_telegram_[this->crypt_bytes_read_] = c;
    this->crypt_bytes_read_++;

    // Read the length of the incoming encrypted telegram.
    if (this->crypt_telegram_len_ == 0 && this->crypt_bytes_read_ > 20) {
      // Complete header + data bytes
      this->crypt_telegram_len_ = 13 + (this->crypt_telegram_[11] << 8 | this->crypt_telegram_[12]);
      ESP_LOGV(TAG, "Encrypted telegram length: %d bytes", this->crypt_telegram_len_);
    }

    // Check for the end of the encrypted telegram.
    if (this->crypt_telegram_len_ == 0 || this->crypt_bytes_read_ != this->crypt_telegram_len_) {
      continue;
    }
    ESP_LOGV(TAG, "End of encrypted telegram found");

    // Decrypt the encrypted telegram.
    GCM<AES128> *gcmaes128{new GCM<AES128>()};
    gcmaes128->setKey(this->decryption_key_.data(), gcmaes128->keySize());
    // the iv is 8 bytes of the system title + 4 bytes frame counter
    // system title is at byte 2 and frame counter at byte 15
    for (int i = 10; i < 14; i++)
      this->crypt_telegram_[i] = this->crypt_telegram_[i + 4];
    constexpr uint16_t iv_size{12};
    gcmaes128->setIV(&this->crypt_telegram_[2], iv_size);
    gcmaes128->decrypt(reinterpret_cast<uint8_t *>(this->telegram_),
                       // the ciphertext start at byte 18
                       &this->crypt_telegram_[18],
                       // cipher size
                       this->crypt_bytes_read_ - 17);
    delete gcmaes128;  // NOLINT(cppcoreguidelines-owning-memory)

    this->bytes_read_ = strnlen(this->telegram_, this->max_telegram_len_);
    ESP_LOGV(TAG, "Decrypted telegram size: %d bytes", this->bytes_read_);
    ESP_LOGVV(TAG, "Decrypted telegram: %s", this->telegram_);

    // Parse the decrypted telegram and publish sensor values.
    this->parse_telegram();
    this->reset_telegram_();
    return;
  }
}

bool Dsmr::parse_telegram() {
  MyData data;
  ESP_LOGV(TAG, "Trying to parse telegram");
  this->stop_requesting_data_();
  ::dsmr::ParseResult<void> res =
      ::dsmr::P1Parser::parse(&data, this->telegram_, this->bytes_read_, false,
                              this->crc_check_);  // Parse telegram according to data definition. Ignore unknown values.
  if (res.err) {
    // Parsing error, show it
    auto err_str = res.fullError(this->telegram_, this->telegram_ + this->bytes_read_);
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
  ESP_LOGCONFIG(TAG, "  Receive timeout: %.1fs", this->receive_timeout_ / 1e3f);
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
    if (this->crypt_telegram_ != nullptr) {
      delete[] this->crypt_telegram_;
      this->crypt_telegram_ = nullptr;
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

  if (this->crypt_telegram_ == nullptr) {
    this->crypt_telegram_ = new uint8_t[this->max_telegram_len_];  // NOLINT
  }
}

}  // namespace dsmr
}  // namespace esphome

#endif  // USE_ARDUINO
