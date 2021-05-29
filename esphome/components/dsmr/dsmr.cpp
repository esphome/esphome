#include "dsmr.h"
#include "esphome/core/log.h"

#include <AES.h>
#include <Crypto.h>
#include <GCM.h>

namespace esphome {
namespace dsmr_ {

static const char *TAG = "dsmr";

void Dsmr::loop() {
  if (this->decryption_key_.size() == 0)
    this->receive_telegram();
  else
    this->receive_encrypted();
}

void Dsmr::receive_telegram() {
  while (available()) {
    const char c = read();

    if (c == '/') {  // header: forward slash
      ESP_LOGV(TAG, "Header found");
      header_found_ = true;
      footer_found_ = false;
      telegram_len_ = 0;
    }

    if (!header_found_)
      continue;
    if (telegram_len_ >= MAX_TELEGRAM_LENGTH) {  // Buffer overflow
      header_found_ = false;
      footer_found_ = false;
      ESP_LOGE(TAG, "Error: Message larger than buffer");
    }

    telegram_[telegram_len_] = c;
    telegram_len_++;
    if (c == '!') {  // footer: exclamation mark
      ESP_LOGV(TAG, "Footer found");
      footer_found_ = true;
    } else {
      if (footer_found_ && c == 10) {  // last \n after footer
        header_found_ = false;
        // Parse message
        if (parse_telegram())
          return;
      }
    }
  }
}

void Dsmr::receive_encrypted() {
  // Encrypted buffer
  uint8_t buffer[MAX_TELEGRAM_LENGTH];
  size_t buffer_length = 0;

  size_t packet_size = 0;
  while (available()) {
    const char c = read();

    if (!header_found_) {
      if (c == 0xdb) {
        ESP_LOGV(TAG, "Start byte 0xDB found");
        header_found_ = true;
      }
    }

    // Sanity check
    if (!header_found_ || buffer_length >= MAX_TELEGRAM_LENGTH) {
      if (buffer_length == 0) {
        ESP_LOGE(TAG, "First byte of encrypted telegram should be 0xDB, aborting.");
      } else {
        ESP_LOGW(TAG, "Unexpected data");
      }
      this->status_momentary_warning("unexpected_data");
      this->flush();
      while (available())
        read();
      return;
    }

    buffer[buffer_length++] = c;

    if (packet_size == 0 && buffer_length > 20)  // Complete header + a few bytes of data
    {
      packet_size = buffer[11] << 8 | buffer[12];
    }
    if (buffer_length == packet_size + 13 && packet_size > 0) {
      ESP_LOGV(TAG, "Encrypted data: %d bytes", buffer_length);

      GCM<AES128> *gcmaes128{new GCM<AES128>()};
      gcmaes128->setKey(this->decryption_key_.data(), gcmaes128->keySize());
      // the iv is 8 bytes of the system title + 4 bytes frame counter
      // system title is at byte 2 and frame counter at byte 15
      for (int i = 10; i < 14; i++)
        buffer[i] = buffer[i + 4];
      constexpr uint16_t iv_size{12};
      gcmaes128->setIV(&buffer[2], iv_size);
      gcmaes128->decrypt(static_cast<uint8_t *>(static_cast<void *>(this->telegram_)),
                         // the cypher text start at byte 18
                         &buffer[18],
                         // cypher data size
                         buffer_length - 17);
      delete gcmaes128;

      telegram_len_ = strlen(this->telegram_);
      ESP_LOGV(TAG, "Decrypted data length: %d", telegram_len_);
      ESP_LOGVV(TAG, "Decrypted data %s", this->telegram_);

      parse_telegram();
      telegram_len_ = 0;
      return;
    }

    if (!available()) {
      // baud rate is 115200 for encrypted data, this means a few byte should arrive every time
      // program runs faster than buffer loading then available() might return false in the middle
      delay(4);  // Wait for data
    }
  }
  if (buffer_length > 0)
    ESP_LOGW(TAG, "Timeout while waiting for encrypted data or invalid data received.");
}

bool Dsmr::parse_telegram() {
  MyData data;
  ESP_LOGV(TAG, "Trying to parse");
  ::dsmr::ParseResult<void> res =
      ::dsmr::P1Parser::parse(&data, telegram_, telegram_len_,
                              false);  // Parse telegram according to data definition. Ignore unknown values.
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
  ESP_LOGCONFIG(TAG, "dsmr:");

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
    ESP_LOGE(TAG, "Error, decryption key must be 32 character long.");
    return;
  }
  this->decryption_key_.clear();

  ESP_LOGI(TAG, "Decryption key is set.");
  // Verbose level prints decryption key
  ESP_LOGV(TAG, "Using decryption key: %s", decryption_key.c_str());

  char temp[3] = {0};
  for (int i = 0; i < 16; i++) {
    strncpy(temp, &(decryption_key.c_str()[i * 2]), 2);
    decryption_key_.push_back(std::strtoul(temp, NULL, 16));
  }
}

}  // namespace dsmr_
}  // namespace esphome
