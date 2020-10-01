#include "pn532_spi.h"
#include "esphome/core/log.h"

// Based on:
// - https://cdn-shop.adafruit.com/datasheets/PN532C106_Application+Note_v1.2.pdf
// - https://www.nxp.com/docs/en/nxp/application-notes/AN133910.pdf
// - https://www.nxp.com/docs/en/nxp/application-notes/153710.pdf

namespace esphome {
namespace pn532_spi {

static const char *TAG = "pn532_spi";

void PN532Spi::setup() {
  ESP_LOGI(TAG, "PN532Spi setup started!");
  this->spi_setup();

  // Wake the chip up from power down
  // 1. Enable the SS line for at least 2ms
  // 2. Send a dummy command to get the protocol synced up
  //    (this may time out, but that's ok)
  // 3. Send SAM config command with normal mode without waiting for ready bit (IRQ not initialized yet)
  // 4. Probably optional, send SAM config again, this time checking ACK and return value
  this->cs_->digital_write(false);
  delay(10);
  ESP_LOGI(TAG, "SPI setup finished!");
  PN532::setup();
}

void PN532Spi::pn532_write_command_(const std::vector<uint8_t> &data) {
  this->enable();
  delay(2);
  // First byte, communication mode: Write data
  this->write_byte(0x01);

  PN532::pn532_write_command_(data);

  this->disable();
}

std::vector<uint8_t> PN532Spi::pn532_read_data_() {
  if (!this->wait_ready_())
    return {};

  this->enable();
  delay(2);
  // Read data (transmission from the PN532 to the host)
  this->write_byte(0x03);

  std::vector<uint8_t> ret = PN532::pn532_read_data_();
  this->disable();
  return ret;
}

bool PN532Spi::is_ready() {
  this->enable();
  // First byte, communication mode: Read state
  this->write_byte(0x02);
  // PN532 returns a single data byte,
  // "After having sent a command, the host controller must wait for bit 0 of Status byte equals 1
  // before reading the data from the PN532."
  std::vector<uint8_t> data = this->pn532_read_bytes(1);
  this->disable();
  return data.size() == 1 && data[0] == 0x01;
}

void PN532Spi::dump_config() {
  PN532::dump_config();
  LOG_PIN("  CS Pin: ", this->cs_);
}

bool PN532Spi::read_ack_() {
  this->enable();
  delay(2);
  // "Read data (transmission from the PN532 to the host) "
  this->write_byte(0x03);

  bool matches = PN532::read_ack_();

  this->disable();
  return matches;
}

}  // namespace pn532_spi
}  // namespace esphome
