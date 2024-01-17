#include "pn7160_spi.h"
#include "esphome/core/log.h"

namespace esphome {
namespace pn7160_spi {

static const char *const TAG = "pn7160_spi";

void PN7160Spi::setup() {
  this->spi_setup();
  this->cs_->digital_write(false);
  PN7160::setup();
}

uint8_t PN7160Spi::read_nfcc(nfc::NciMessage &rx, const uint16_t timeout) {
  if (this->wait_for_irq_(timeout) != nfc::STATUS_OK) {
    ESP_LOGW(TAG, "read_nfcc_() timeout waiting for IRQ");
    return nfc::STATUS_FAILED;
  }

  rx.get_message().resize(nfc::NCI_PKT_HEADER_SIZE);
  this->enable();
  this->write_byte(TDD_SPI_READ);  // send "transfer direction detector"
  this->read_array(rx.get_message().data(), nfc::NCI_PKT_HEADER_SIZE);

  uint8_t length = rx.get_payload_size();
  if (length > 0) {
    rx.get_message().resize(length + nfc::NCI_PKT_HEADER_SIZE);
    this->read_array(rx.get_message().data() + nfc::NCI_PKT_HEADER_SIZE, length);
  }
  this->disable();
  // semaphore to ensure transaction is complete before returning
  if (this->wait_for_irq_(pn7160::NFCC_DEFAULT_TIMEOUT, false) != nfc::STATUS_OK) {
    ESP_LOGW(TAG, "read_nfcc_() post-read timeout waiting for IRQ line to clear");
    return nfc::STATUS_FAILED;
  }
  return nfc::STATUS_OK;
}

uint8_t PN7160Spi::write_nfcc(nfc::NciMessage &tx) {
  this->enable();
  this->write_byte(TDD_SPI_WRITE);  // send "transfer direction detector"
  this->write_array(tx.encode().data(), tx.encode().size());
  this->disable();
  return nfc::STATUS_OK;
}

void PN7160Spi::dump_config() {
  PN7160::dump_config();
  LOG_PIN("  CS Pin: ", this->cs_);
}

}  // namespace pn7160_spi
}  // namespace esphome
