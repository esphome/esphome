#include "pn532_i2c.h"
#include "esphome/core/log.h"

// Based on:
// - https://cdn-shop.adafruit.com/datasheets/PN532C106_Application+Note_v1.2.pdf
// - https://www.nxp.com/docs/en/nxp/application-notes/AN133910.pdf
// - https://www.nxp.com/docs/en/nxp/application-notes/153710.pdf

namespace esphome {
namespace pn532_i2c {

static const char *TAG = "pn532_i2c";

std::vector<uint8_t> PN532I2C::pn532_read_data() {
  if (!this->wait_ready_())
    return {};

  return PN532::pn532_read_data();
}

bool PN532I2C::is_ready() {
  // PN532 returns a single data byte,
  // "After having sent a command, the host controller must wait for bit 0 of Status byte equals 1
  // before reading the data from the PN532."
  std::vector<uint8_t> data = this->pn532_read_bytes(1);

  return data.size() == 1 && data[0] == 0x01;
}

void PN532I2C::dump_config() {
  PN532::dump_config();
  LOG_I2C_DEVICE(this);
}

}  // namespace pn532_i2c
}  // namespace esphome
