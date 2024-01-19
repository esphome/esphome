#include "pn532_spi.h"
#include "esphome/core/log.h"

// Based on:
// - https://cdn-shop.adafruit.com/datasheets/PN532C106_Application+Note_v1.2.pdf
// - https://www.nxp.com/docs/en/nxp/application-notes/AN133910.pdf
// - https://www.nxp.com/docs/en/nxp/application-notes/153710.pdf

namespace esphome {
namespace pn532_spi {

static const char *const TAG = "pn532_spi";

void PN532Spi::setup() {
  ESP_LOGI(TAG, "PN532Spi setup started!");
  this->spi_setup();

  this->cs_->digital_write(false);
  delay(10);
  ESP_LOGI(TAG, "SPI setup finished!");
  PN532::setup();
}

bool PN532Spi::is_read_ready() {
  this->enable();
  this->write_byte(0x02);
  bool ready = this->read_byte() == 0x01;
  this->disable();
  return ready;
}

bool PN532Spi::write_data(const std::vector<uint8_t> &data) {
  this->enable();
  delay(2);
  // First byte, communication mode: Write data
  this->write_byte(0x01);
  ESP_LOGV(TAG, "Writing data: %s", format_hex_pretty(data).c_str());
  this->write_array(data.data(), data.size());
  this->disable();

  return true;
}

bool PN532Spi::read_data(std::vector<uint8_t> &data, uint8_t len) {
  if (this->read_ready_(true) != pn532::PN532ReadReady::READY) {
    return false;
  }

  // Read data (transmission from the PN532 to the host)
  this->enable();
  delay(2);
  this->write_byte(0x03);

  ESP_LOGV(TAG, "Reading data...");

  data.resize(len);
  this->read_array(data.data(), len);
  this->disable();
  data.insert(data.begin(), 0x01);
  ESP_LOGV(TAG, "Read data: %s", format_hex_pretty(data).c_str());
  return true;
}

bool PN532Spi::read_response(uint8_t command, std::vector<uint8_t> &data) {
  ESP_LOGV(TAG, "Reading response");

  if (this->read_ready_(true) != pn532::PN532ReadReady::READY) {
    return false;
  }

  this->enable();
  delay(2);
  this->write_byte(0x03);

  std::vector<uint8_t> header(7);
  this->read_array(header.data(), 7);

  ESP_LOGV(TAG, "Header data: %s", format_hex_pretty(header).c_str());

  if (header[0] != 0x00 && header[1] != 0x00 && header[2] != 0xFF) {
    // invalid packet
    ESP_LOGV(TAG, "read data invalid preamble!");
    return false;
  }

  bool valid_header = (static_cast<uint8_t>(header[3] + header[4]) == 0 &&  // LCS, len + lcs = 0
                       header[5] == 0xD5 &&        // TFI - frame from PN532 to system controller
                       header[6] == command + 1);  // Correct command response

  if (!valid_header) {
    ESP_LOGV(TAG, "read data invalid header!");
    return false;
  }

  // full length of message, including command response
  uint8_t full_len = header[3];
  // length of data, excluding command response
  uint8_t len = full_len - 1;
  if (full_len == 0)
    len = 0;

  ESP_LOGV(TAG, "Reading response of length %d", len);

  data.resize(len + 1);
  this->read_array(data.data(), len + 1);
  this->disable();

  ESP_LOGV(TAG, "Response data: %s", format_hex_pretty(data).c_str());

  uint8_t checksum = header[5] + header[6];  // TFI + Command response code
  for (int i = 0; i < len - 1; i++) {
    uint8_t dat = data[i];
    checksum += dat;
  }
  checksum = ~checksum + 1;

  if (data[len - 1] != checksum) {
    ESP_LOGV(TAG, "read data invalid checksum! %02X != %02X", data[len - 1], checksum);
    return false;
  }

  if (data[len] != 0x00) {
    ESP_LOGV(TAG, "read data invalid postamble!");
    return false;
  }

  data.erase(data.end() - 2, data.end());  // Remove checksum and postamble

  return true;
}

void PN532Spi::dump_config() {
  PN532::dump_config();
  LOG_PIN("  CS Pin: ", this->cs_);
}

}  // namespace pn532_spi
}  // namespace esphome
