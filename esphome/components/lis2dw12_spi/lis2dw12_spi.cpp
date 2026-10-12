#include "lis2dw12_spi.h"
#include "esphome/core/log.h"

namespace esphome::lis2dw12_spi {

ESPHOME_LOG_TAG(TAG, "lis2dw12_spi");

void LIS2DW12SPIComponent::setup() {
  this->spi_setup();
  lis2dw12::LIS2DW12Component::setup();
}

void LIS2DW12SPIComponent::dump_config() {
  LOG_SPI_DEVICE(this);
  lis2dw12::LIS2DW12Component::dump_config();
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
  this->read_array(data, len);
  this->disable();
  return true;
}

}  // namespace esphome::lis2dw12_spi
