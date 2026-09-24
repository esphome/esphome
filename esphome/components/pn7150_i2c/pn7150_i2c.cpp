#include "pn7150_i2c.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome::pn7150_i2c {

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
  // IRQ normally drops at the end of the read. If another message is queued it rises again at once, and the short
  // low pulse may be missed; that means more data is waiting, not that this read failed (UM10936, 3.4).
  if (this->wait_for_irq_(pn71xx::NFCC_DEFAULT_TIMEOUT, false) != nfc::STATUS_OK) {
    ESP_LOGVV(TAG, "IRQ still active after read; another message is pending");
  }
  return nfc::STATUS_OK;
}

uint8_t PN7150I2C::write_nfcc(nfc::NciMessage &tx) {
  auto encoded = tx.encode();
  if (this->write(encoded.data(), encoded.size()) == i2c::ERROR_OK) {
    return nfc::STATUS_OK;
  }
  return nfc::STATUS_FAILED;
}

void PN7150I2C::dump_config() {
  PN7150::dump_config();
  LOG_I2C_DEVICE(this);
}

}  // namespace esphome::pn7150_i2c
