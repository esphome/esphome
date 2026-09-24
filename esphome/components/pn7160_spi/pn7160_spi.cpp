#include "pn7160_spi.h"
#include "esphome/core/log.h"

namespace esphome::pn7160_spi {

static const char *const TAG = "pn7160_spi";

void PN7160Spi::setup() {
  this->spi_setup();
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
  // IRQ normally drops at the end of the read. If another message is queued it rises again at once, and the short
  // low pulse may be missed; that means more data is waiting, not that this read failed (UM11495, 6.3.4).
  if (this->wait_for_irq_(pn71xx::NFCC_DEFAULT_TIMEOUT, false) != nfc::STATUS_OK) {
    ESP_LOGVV(TAG, "IRQ still active after read; another message is pending");
  }
  return nfc::STATUS_OK;
}

uint8_t PN7160Spi::write_nfcc(nfc::NciMessage &tx) {
  auto encoded = tx.encode();
  this->enable();
  // send "transfer direction detector"; the NFCC answers 0xFF when it is ready to receive (UM11495, 6.3.3)
  const bool ready = this->transfer_byte(TDD_SPI_WRITE) == 0xFF;
  if (ready) {
    this->write_array(encoded.data(), encoded.size());
  }
  this->disable();
  return ready ? nfc::STATUS_OK : nfc::STATUS_FAILED;
}

void PN7160Spi::dump_config() {
  PN7160::dump_config();
  LOG_PIN("  CS Pin: ", this->cs_);
}

}  // namespace esphome::pn7160_spi
