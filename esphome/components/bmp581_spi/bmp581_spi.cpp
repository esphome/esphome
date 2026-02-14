#include <cstdint>
#include <cstddef>

#include "bmp581_spi.h"
#include <esphome/components/bmp581_base/bmp581_base.h>

namespace esphome::bmp581_spi {

static const char *const TAG = "bmp581_spi";

uint8_t set_bit(uint8_t num, uint8_t position) {
  uint8_t mask = 1 << position;
  return num | mask;
}

uint8_t clear_bit(uint8_t num, uint8_t position) {
  uint8_t mask = 1 << position;
  return num & ~mask;
}

void BMP581SPIComponent::setup() {
  this->spi_setup();
  BMP581Component::setup();
}

void BMP581SPIComponent::activate_protocol() {
  // - forces the device into SPI mode using a dummy read
  uint8_t dummy_read = 0;
  this->bmp_read_byte(bmp581_base::BMP581_CHIP_ID, &dummy_read);
}

// In SPI mode, only 7 bits of the register addresses are used; the MSB of register address is not used
// and replaced by a read/write bit (RW = ‘0’ for write and RW = ‘1’ for read).
// Example: address 0xF7 is accessed by using SPI register address 0x77. For write access, the byte
// 0x77 is transferred, for read access, the byte 0xF7 is transferred.
// https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bmp581-ds004.pdf

bool BMP581SPIComponent::bmp_read_byte(uint8_t a_register, uint8_t *data) {
  this->enable();
  this->transfer_byte(set_bit(a_register, 7));
  *data = this->transfer_byte(0);
  this->disable();
  return true;
}

bool BMP581SPIComponent::bmp_write_byte(uint8_t a_register, uint8_t data) {
  this->enable();
  this->transfer_byte(clear_bit(a_register, 7));
  this->transfer_byte(data);
  this->disable();
  return true;
}

bool BMP581SPIComponent::bmp_read_bytes(uint8_t a_register, uint8_t *data, size_t len) {
  this->enable();
  this->transfer_byte(set_bit(a_register, 7));
  this->read_array(data, len);
  this->disable();
  return true;
}

bool BMP581SPIComponent::bmp_write_bytes(uint8_t a_register, uint8_t *data, size_t len) {
  this->enable();
  this->transfer_byte(set_bit(a_register, 7));
  this->write_array(data, len);
  this->disable();
  return true;
}
}  // namespace esphome::bmp581_spi
