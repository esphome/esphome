#include <cstdint>
#include <cstddef>

#include "bme280_spi.h"
#include <esphome/components/bme280_base/bme280_base.h>

int set_bit(uint8_t num, int position) {
  int mask = 1 << position;
  return num | mask;
}

int clear_bit(uint8_t num, int position) {
  int mask = 1 << position;
  return num & ~mask;
}

namespace esphome {
namespace bme280_spi {

void BME280SPIComponent::setup() {
  this->spi_setup();
  BME280Component::setup();
};

// In SPI mode, only 7 bits of the register addresses are used; the MSB of register address is not used
// and replaced by a read/write bit (RW = ‘0’ for write and RW = ‘1’ for read).
// Example: address 0xF7 is accessed by using SPI register address 0x77. For write access, the byte
// 0x77 is transferred, for read access, the byte 0xF7 is transferred.
// https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bme280-ds002.pdf

bool BME280SPIComponent::read_byte(uint8_t a_register, uint8_t *data) {
  this->enable();
  // cause: *data = this->delegate_->transfer(tmp) doesnt work
  this->delegate_->transfer(set_bit(a_register, 7));
  *data = this->delegate_->transfer(0);
  this->disable();
  return true;
}

bool BME280SPIComponent::write_byte(uint8_t a_register, uint8_t data) {
  this->enable();
  this->delegate_->transfer(clear_bit(a_register, 7));
  this->delegate_->transfer(data);
  this->disable();
  return true;
}

bool BME280SPIComponent::read_bytes(uint8_t a_register, uint8_t *data, size_t len) {
  this->enable();
  this->delegate_->transfer(set_bit(a_register, 7));
  this->delegate_->read_array(data, len);
  this->disable();
  return true;
}

bool BME280SPIComponent::read_byte_16(uint8_t a_register, uint16_t *data) {
  this->enable();
  this->delegate_->transfer(set_bit(a_register, 7));
  ((uint8_t *) data)[1] = this->delegate_->transfer(0);
  ((uint8_t *) data)[0] = this->delegate_->transfer(0);
  this->disable();
  return true;
}

}  // namespace bme280_spi
}  // namespace esphome
