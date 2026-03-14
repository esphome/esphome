#include "lis2dh12_spi.h"
#include "esphome/core/log.h"

namespace esphome::lis2dh12_spi {

static const char *const TAG = "lis2dh12_spi";

// LIS2DH12 SPI protocol:
// Read:  bit 7 = 1 (read), bit 6 = MS (auto-increment for multi-byte)
// Write: bit 7 = 0 (write), bit 6 = MS

void LIS2DH12SPIComponent::setup() {
  this->spi_setup();
  lis2dh12_base::LIS2DH12Component::setup();
}

void LIS2DH12SPIComponent::dump_config() {
  LOG_PIN("  CS Pin:", this->cs_);
  lis2dh12_base::LIS2DH12Component::dump_config();
}

bool LIS2DH12SPIComponent::read_byte(uint8_t a_register, uint8_t *data) {
  this->enable();
  this->transfer_byte(a_register | 0x80);  // Read bit
  *data = this->transfer_byte(0);
  this->disable();
  return true;
}

bool LIS2DH12SPIComponent::write_byte(uint8_t a_register, uint8_t data) {
  this->enable();
  this->transfer_byte(a_register & 0x3F);  // Write: clear bits 7 and 6
  this->transfer_byte(data);
  this->disable();
  return true;
}

bool LIS2DH12SPIComponent::read_bytes(uint8_t a_register, uint8_t *data, size_t len) {
  this->enable();
  this->transfer_byte(a_register | 0xC0);  // Read bit + auto-increment bit
  for (size_t i = 0; i < len; i++) {
    data[i] = this->transfer_byte(0);
  }
  this->disable();
  return true;
}

}  // namespace esphome::lis2dh12_spi
