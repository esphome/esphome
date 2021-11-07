#ifdef USE_ARDUINO

#include "dsmr.h"
#include "esphome/core/log.h"

#include <AES.h>
#include <Crypto.h>
#include <GCM.h>

namespace esphome {
namespace dsmr {

static const char *const TAG = "dsmr";

void Dsmr::loop() {
  if (this->decryption_key_.empty())
    this->receive_telegram_();
  else
    this->receive_encrypted_();
}

bool Dsmr::available_within_timeout_() {
  uint8_t tries = READ_TIMEOUT_MS / 5;
  while (tries--) {
    delay(5);
    if (available()) {
      return true;
    }
  }
  return false;
}

void Dsmr::receive_telegram_() {
  while (true) {
    if (!available()) {
      if (!header_found_ || !available_within_timeout_()) {
        return;
      }
    }

    const char c = read();

    // Find a new telegram header, i.e. forward slash.
    if (c == '/') {
      ESP_LOGV(TAG, "Header of telegram found");
      header_found_ = true;
      footer_found_ = false;
      telegram_len_ = 0;
    }
    if (!header_found_)
      continue;

    // Check for buffer overflow.
    if (telegram_len_ >= MAX_TELEGRAM_LENGTH) {
      header_found_ = false;
      footer_found_ = false;
      ESP_LOGE(TAG, "Error: telegram larger than buffer (%d bytes)", MAX_TELEGRAM_LENGTH);
      return;
    }

    // Some v2.2 or v3 meters will send a new value which starts with '('
    // in a new line while the value belongs to the previous ObisId. For
    // proper parsing remove these new line characters
    while (c == '(' && (telegram_[telegram_len_ - 1] == '\n' || telegram_[telegram_len_ - 1] == '\r'))
      telegram_len_--;

    // Store the byte in the buffer.
    telegram_[telegram_len_] = c;
    telegram_len_++;

    // Check for a footer, i.e. exlamation mark, followed by a hex checksum.
    if (c == '!') {
      ESP_LOGV(TAG, "Footer of telegram found");
      footer_found_ = true;
      continue;
    }
    // Check for the end of the hex checksum, i.e. a newline.
    if (footer_found_ && c == '\n') {
      // Parse the telegram and publish sensor values.
      parse_telegram();

      header_found_ = false;
      return;
    }
  }
}

void Dsmr::receive_encrypted_() {
  // Encrypted buffer
  uint8_t buffer[MAX_TELEGRAM_LENGTH];
  size_t buffer_length = 0;
  size_t packet_size = 0;

  while (true) {
    if (!available()) {
      if (!header_found_) {
        return;
      }
      if (!available_within_timeout_()) {
        ESP_LOGW(TAG, "Timeout while reading data for encrypted telegram");
        return;
      }
    }

    const char c = read();

    // Find a new telegram start byte.
    if (!header_found_) {
      if ((uint8_t) c != 0xDB) {
        continue;
      }
      ESP_LOGV(TAG, "Start byte 0xDB of encrypted telegram found");
      header_found_ = true;
    }

    // Check for buffer overflow.
    if (buffer_length >= MAX_TELEGRAM_LENGTH) {
      header_found_ = false;
      ESP_LOGE(TAG, "Error: encrypted telegram larger than buffer (%d bytes)", MAX_TELEGRAM_LENGTH);
      return;
    }

    buffer[buffer_length++] = c;

    if (packet_size == 0 && buffer_length > 20) {
      // Complete header + data bytes
      packet_size = 13 + (buffer[11] << 8 | buffer[12]);
      ESP_LOGV(TAG, "Encrypted telegram size: %d bytes", packet_size);
    }
    if (buffer_length == packet_size && packet_size > 0) {
      ESP_LOGV(TAG, "End of encrypted telegram found");
      GCM<AES128> *gcmaes128{new GCM<AES128>()};
      gcmaes128->setKey(this->decryption_key_.data(), gcmaes128->keySize());
      // the iv is 8 bytes of the system title + 4 bytes frame counter
      // system title is at byte 2 and frame counter at byte 15
      for (int i = 10; i < 14; i++)
        buffer[i] = buffer[i + 4];
      constexpr uint16_t iv_size{12};
      gcmaes128->setIV(&buffer[2], iv_size);
      gcmaes128->decrypt(reinterpret_cast<uint8_t *>(this->telegram_),
                         // the ciphertext start at byte 18
                         &buffer[18],
                         // cipher size
                         buffer_length - 17);
      delete gcmaes128;  // NOLINT(cppcoreguidelines-owning-memory)

      telegram_len_ = strnlen(this->telegram_, sizeof(this->telegram_));
      ESP_LOGV(TAG, "Decrypted telegram size: %d bytes", telegram_len_);
      ESP_LOGVV(TAG, "Decrypted telegram: %s", this->telegram_);

      parse_telegram();

      header_found_ = false;
      telegram_len_ = 0;
      return;
    }
  }
}

bool Dsmr::parse_telegram() {
  MyData data;
  ESP_LOGV(TAG, "Trying to parse telegram");
  ::dsmr::ParseResult<void> res =
      ::dsmr::P1Parser::parse(&data, telegram_, telegram_len_, false,
                              this->crc_check_);  // Parse telegram according to data definition. Ignore unknown values.
  if (res.err) {
    // Parsing error, show it
    auto err_str = res.fullError(telegram_, telegram_ + telegram_len_);
    ESP_LOGE(TAG, "%s", err_str.c_str());
    return false;
  } else {
    this->status_clear_warning();
    publish_sensors(data);
    return true;
  }
}

void Dsmr::dump_config() {
  ESP_LOGCONFIG(TAG, "DSMR:");

#define DSMR_LOG_SENSOR(s) LOG_SENSOR("  ", #s, this->s_##s##_);
  DSMR_SENSOR_LIST(DSMR_LOG_SENSOR, )

#define DSMR_LOG_TEXT_SENSOR(s) LOG_TEXT_SENSOR("  ", #s, this->s_##s##_);
  DSMR_TEXT_SENSOR_LIST(DSMR_LOG_TEXT_SENSOR, )
}

void Dsmr::set_decryption_key(const std::string &decryption_key) {
  if (decryption_key.length() == 0) {
    ESP_LOGI(TAG, "Disabling decryption");
    this->decryption_key_.clear();
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
    decryption_key_.push_back(std::strtoul(temp, nullptr, 16));
  }
}

}  // namespace dsmr
}  // namespace esphome

#endif  // USE_ARDUINO
