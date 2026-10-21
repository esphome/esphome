#include "pn532_i2c.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

#include <algorithm>

// Based on:
// - https://cdn-shop.adafruit.com/datasheets/PN532C106_Application+Note_v1.2.pdf
// - https://www.nxp.com/docs/en/nxp/application-notes/AN133910.pdf
// - https://www.nxp.com/docs/en/nxp/application-notes/153710.pdf

namespace esphome::pn532_i2c {

static const char *const TAG = "pn532_i2c";

bool PN532I2C::is_read_ready() {
  uint8_t status;
  if (!this->read_bytes_raw(&status, 1)) {
    return false;
  }
  // only bit 0 (RDY) of the status byte is defined (UM0701-02, 6.2.4)
  return status & 0x01;
}

bool PN532I2C::write_data(const std::span<const uint8_t> data) {
  return this->write(data.data(), data.size()) == i2c::ERROR_OK;
}

bool PN532I2C::read_data(pn532::PN532Frame &data, size_t len) {
  if (len + 1 > pn532::PN532_FRAME_MAX_SIZE) {
    return false;
  }
  delay(1);

  if (this->read_ready_(true) != pn532::PN532ReadReady::READY) {
    return false;
  }

  // the PN532 prefixes every frame with a status byte
  data.resize(len + 1);
  return this->read_bytes_raw(data.data(), len + 1);
}

bool PN532I2C::read_response(uint8_t command, pn532::PN532Frame &data) {
  ESP_LOGV(TAG, "Reading response");
  uint8_t len = this->read_response_length_();
  if (len == 0) {
    return false;
  }

  ESP_LOGV(TAG, "Reading response of length %d", len);
  if (!this->read_data(data, 6 + len + 2)) {
    ESP_LOGD(TAG, "No response data");
    return false;
  }

  if (data[1] != 0x00 || data[2] != 0x00 || data[3] != 0xFF) {
    // invalid packet
    ESP_LOGV(TAG, "read data invalid preamble!");
    return false;
  }

  bool valid_header = (static_cast<uint8_t>(data[4] + data[5]) == 0 &&  // LCS, len + lcs = 0
                       data[6] == 0xD5 &&                               // TFI - frame from PN532 to system controller
                       data[7] == command + 1);                         // Correct command response

  if (!valid_header) {
    ESP_LOGV(TAG, "read data invalid header!");
    return false;
  }

  // frame: status, preamble, start code (2), LEN, LCS, TFI, command response code, data, DCS, postamble
  constexpr size_t tfi_offset = 6;
  uint8_t checksum = 0;
  for (size_t i = 0; i < len + 1U; i++) {
    checksum += data[tfi_offset + i];
  }
  checksum = ~checksum + 1;

  if (data[tfi_offset + len + 1] != checksum) {
    ESP_LOGV(TAG, "read data invalid checksum! %02X != %02X", data[tfi_offset + len + 1], checksum);
    return false;
  }

  if (data[tfi_offset + len + 2] != 0x00) {
    ESP_LOGV(TAG, "read data invalid postamble!");
    return false;
  }

  // keep only the data bytes that follow the command response code
  std::copy(data.begin() + tfi_offset + 2, data.begin() + tfi_offset + len + 1, data.begin());
  data.resize(len - 1);

  return true;
}

uint8_t PN532I2C::read_response_length_() {
  pn532::PN532Frame data;
  if (!this->read_data(data, 6)) {
    return 0;
  }

  if (data[1] != 0x00 || data[2] != 0x00 || data[3] != 0xFF) {
    // invalid packet
    ESP_LOGV(TAG, "read data invalid preamble!");
    return 0;
  }

  bool valid_header = (static_cast<uint8_t>(data[4] + data[5]) == 0 &&  // LCS, len + lcs = 0
                       data[6] == 0xD5);                                // TFI - frame from PN532 to system controller

  if (!valid_header) {
    ESP_LOGV(TAG, "read data invalid header!");
    return 0;
  }

  this->send_nack_();

  // full length of message, including TFI
  uint8_t full_len = data[4];
  // length of data, excluding TFI
  uint8_t len = full_len - 1;
  if (full_len == 0)
    len = 0;
  return len;
}

void PN532I2C::dump_config() {
  PN532::dump_config();
  LOG_I2C_DEVICE(this);
}

}  // namespace esphome::pn532_i2c
