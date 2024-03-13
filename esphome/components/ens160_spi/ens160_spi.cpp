#include <cstdint>
#include <cstddef>

#include "ens160_spi.h"
#include <esphome/components/ens160_base/ens160_base.h>

int set_bit(uint8_t num, int position) {
  int mask = 1 << position;
  return num | mask;
}

int clear_bit(uint8_t num, int position) {
  int mask = 1 << position;
  return num & ~mask;
}

namespace esphome {
namespace ens160_spi {

void ENS160SPIComponent::setup() {
  this->spi_setup();
  ENS160Component::setup();
};

// In SPI mode, only 7 bits of the register addresses are used; the MSB of register address is not used
// and replaced by a read/write bit (RW = ‘0’ for write and RW = ‘1’ for read).
// Example: address 0xF7 is accessed by using SPI register address 0x77. For write access, the byte
// 0x77 is transferred, for read access, the byte 0xF7 is transferred.
// https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-ens160-ds002.pdf

bool ENS160SPIComponent::read_byte(uint8_t a_register, uint8_t *data) {
  this->enable();
  // cause: *data = this->delegate_->transfer(tmp) doesnt work
  this->delegate_->transfer(set_bit(a_register, 7));
  *data = this->delegate_->transfer(0);
  this->disable();
  return true;
}

bool ENS160SPIComponent::write_byte(uint8_t a_register, uint8_t data) {
  this->enable();
  this->delegate_->transfer(clear_bit(a_register, 7));
  this->delegate_->transfer(data);
  this->disable();
  return true;
}

bool ENS160SPIComponent::read_bytes(uint8_t a_register, uint8_t *data, size_t len) {
  this->enable();
  this->delegate_->transfer(set_bit(a_register, 7));
  this->delegate_->read_array(data, len);
  this->disable();
  return true;
}

bool ENS160SPIComponent::write_bytes(uint8_t a_register, uint8_t *data, size_t len) {
  this->enable();
  this->delegate_->transfer(clear_bit(a_register, 7));
  this->delegate_->transfer(data, len);
  this->disable();
  return true;
}

}  // namespace ens160_spi
}  // namespace esphome
