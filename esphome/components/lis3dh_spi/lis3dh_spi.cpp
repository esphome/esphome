#include "lis3dh_spi.h"
#include "esphome/core/log.h"

namespace esphome {
namespace lis3dh_spi {

static const char *const TAG = "lis3dh_spi";

void LIS3DHSPI::dump_config() {
  LIS3DHComponent::dump_config();
  LOG_PIN("  CS pin: ", this->cs_);
}

bool LIS3DHSPI::read_register(uint8_t reg, uint8_t *data, uint16_t len) {
  // Bit 7 = read, bit 6 = address auto-increment for multi-byte transfers.
  uint8_t cmd = reg | 0x80;
  if (len > 1) {
    cmd |= 0x40;
  }
  this->enable();
  this->write_byte(cmd);
  this->read_array(data, len);
  this->disable();
  return true;
}

bool LIS3DHSPI::write_register(uint8_t reg, const uint8_t *data, uint16_t len) {
  uint8_t cmd = reg & 0x7F;
  if (len > 1) {
    cmd |= 0x40;
  }
  this->enable();
  this->write_byte(cmd);
  this->write_array(data, len);
  this->disable();
  return true;
}

}  // namespace lis3dh_spi
}  // namespace esphome
