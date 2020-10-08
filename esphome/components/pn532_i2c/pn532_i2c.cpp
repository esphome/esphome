#include "pn532_i2c.h"
#include "esphome/core/log.h"

// Based on:
// - https://cdn-shop.adafruit.com/datasheets/PN532C106_Application+Note_v1.2.pdf
// - https://www.nxp.com/docs/en/nxp/application-notes/AN133910.pdf
// - https://www.nxp.com/docs/en/nxp/application-notes/153710.pdf

namespace esphome {
namespace pn532_i2c {

static const char *TAG = "pn532_i2c";

bool PN532I2C::write_data(const std::vector<uint8_t> &data) { return this->write_bytes_raw(data.data(), data.size()); }

bool PN532I2C::read_data(std::vector<uint8_t> &data, uint8_t len) {
  delay(5);

  std::vector<uint8_t> ready;
  ready.resize(1);
  uint32_t start_time = millis();
  while (true) {
    if (this->read_bytes_raw(ready.data(), 1)) {
      if (ready[0] == 0x01)
        break;
    }

    if (millis() - start_time > 100) {
      ESP_LOGV(TAG, "Timed out waiting for readiness from PN532!");
      return false;
    }
  }

  data.resize(len + 1);
  this->read_bytes_raw(data.data(), len + 1);
  return true;
}

void PN532I2C::dump_config() {
  PN532::dump_config();
  LOG_I2C_DEVICE(this);
}

}  // namespace pn532_i2c
}  // namespace esphome
