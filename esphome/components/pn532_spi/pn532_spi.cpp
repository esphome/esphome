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

  this->cs_->digital_write(false);
  delay(10);
  ESP_LOGI(TAG, "SPI setup finished!");
  PN532::setup();
}

bool PN532Spi::write_data(const std::vector<uint8_t> &data) {
  this->enable();
  delay(2);
  // First byte, communication mode: Write data
  this->write_byte(0x01);

  this->write_array(data.data(), data.size());
  this->disable();

  return true;
}

bool PN532Spi::read_data(std::vector<uint8_t> &data, uint8_t len) {
  this->enable();
  // First byte, communication mode: Read state
  this->write_byte(0x02);

  uint32_t start_time = millis();
  while (true) {
    if (this->read_byte() & 0x01)
      break;

    if (millis() - start_time > 100) {
      this->disable();
      ESP_LOGV(TAG, "Timed out waiting for readiness from PN532!");
      return false;
    }
  }

  // Read data (transmission from the PN532 to the host)
  this->write_byte(0x03);

  data.resize(len);
  this->read_array(data.data(), len);
  this->disable();
  data.insert(data.begin(), 0x01);
  return true;
};

void PN532Spi::dump_config() {
  PN532::dump_config();
  LOG_PIN("  CS Pin: ", this->cs_);
}

}  // namespace pn532_spi
}  // namespace esphome
