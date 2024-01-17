#include "pn7150_i2c.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace pn7150_i2c {

static const char *const TAG = "pn7150_i2c";

uint8_t PN7150I2C::read_nfcc(nfc::NciMessage &rx, const uint16_t timeout) {
  if (this->wait_for_irq_(timeout) != nfc::STATUS_OK) {
    ESP_LOGW(TAG, "read_nfcc_() timeout waiting for IRQ");
    return nfc::STATUS_FAILED;
  }

  rx.get_message().resize(nfc::NCI_PKT_HEADER_SIZE);
  if (!this->read_bytes_raw(rx.get_message().data(), nfc::NCI_PKT_HEADER_SIZE)) {
    return nfc::STATUS_FAILED;
  }

  uint8_t length = rx.get_payload_size();
  if (length > 0) {
    rx.get_message().resize(length + nfc::NCI_PKT_HEADER_SIZE);
    if (!this->read_bytes_raw(rx.get_message().data() + nfc::NCI_PKT_HEADER_SIZE, length)) {
      return nfc::STATUS_FAILED;
    }
  }
  // semaphore to ensure transaction is complete before returning
  if (this->wait_for_irq_(pn7150::NFCC_DEFAULT_TIMEOUT, false) != nfc::STATUS_OK) {
    ESP_LOGW(TAG, "read_nfcc_() post-read timeout waiting for IRQ line to clear");
    return nfc::STATUS_FAILED;
  }
  return nfc::STATUS_OK;
}

uint8_t PN7150I2C::write_nfcc(nfc::NciMessage &tx) {
  if (this->write(tx.encode().data(), tx.encode().size()) == i2c::ERROR_OK) {
    return nfc::STATUS_OK;
  }
  return nfc::STATUS_FAILED;
}

void PN7150I2C::dump_config() {
  PN7150::dump_config();
  LOG_I2C_DEVICE(this);
}

}  // namespace pn7150_i2c
}  // namespace esphome
