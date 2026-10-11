#include "lis2dw12_spi.h"
#include "esphome/core/log.h"

namespace esphome::lis2dw12_spi {

static const char *const TAG = "lis2dw12_spi";

void LIS2DW12SPIComponent::setup() {
  this->spi_setup();
  lis2dw12_base::LIS2DW12Component::setup();
}

void LIS2DW12SPIComponent::dump_config() {
  LOG_PIN("  CS Pin:", this->cs_);
  lis2dw12_base::LIS2DW12Component::dump_config();
}

bool LIS2DW12SPIComponent::read_byte(uint8_t a_register, uint8_t *data) {
  this->enable();
  this->transfer_byte(a_register | 0x80);
  *data = this->transfer_byte(0);
  this->disable();
  return true;
}

bool LIS2DW12SPIComponent::write_byte(uint8_t a_register, uint8_t data) {
  this->enable();
  this->transfer_byte(a_register & 0x3F);
  this->transfer_byte(data);
  this->disable();
  return true;
}

bool LIS2DW12SPIComponent::read_bytes(uint8_t a_register, uint8_t *data, size_t len) {
  this->enable();
  this->transfer_byte(a_register | 0x80);
  for (size_t i = 0; i < len; i++) {
    data[i] = this->transfer_byte(0);
  }
  this->disable();
  return true;
}

}  // namespace esphome::lis2dw12_spi
