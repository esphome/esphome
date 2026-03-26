#include <cstdint>
#include <cstddef>

#include "bmp581_spi.h"
#include "esphome/components/bmp581_base/bmp581_base.h"
#include "esphome/components/spi/spi.h"

namespace esphome::bmp581_spi {

static const char *const TAG = "bmp581_spi";

// OR (|) register with BMP_SPI_READ for read
inline constexpr uint8_t BMP_SPI_READ = 0x80;

// AND (&) register with BMP_SPI_WRITE for write
inline constexpr uint8_t BMP_SPI_WRITE = 0x7F;

void BMP581SPIComponent::dump_config() {
  BMP581Component::dump_config();
  LOG_SPI_DEVICE(this);
}

void BMP581SPIComponent::setup() {
  this->spi_setup();
  BMP581Component::setup();
}

void BMP581SPIComponent::activate_interface() {
  // - forces the device into SPI mode using a dummy read
  uint8_t dummy_read = 0;
  this->bmp_read_byte(bmp581_base::BMP581_CHIP_ID, &dummy_read);
}

// In SPI mode, only 7 bits of the register addresses are used; the MSB of register address is not used
// and replaced by a read/write bit (RW = ‘0’ for write and RW = ‘1’ for read).
// Example: address 0xF7 is accessed by using SPI register address 0x77. For write access, the byte
// 0x77 is transferred, for read access, the byte 0xF7 is transferred.
// The expressions BMP_SPI_READ (| with register) and BMP_SPI_WRITE (& with register)
// are defined for readability.
// https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bmp581-ds004.pdf

bool BMP581SPIComponent::bmp_read_byte(uint8_t a_register, uint8_t *data) {
  this->enable();
  this->transfer_byte(a_register | BMP_SPI_READ);
  *data = this->transfer_byte(0);
  this->disable();
  return true;
}

bool BMP581SPIComponent::bmp_write_byte(uint8_t a_register, uint8_t data) {
  this->enable();
  this->transfer_byte(a_register & BMP_SPI_WRITE);
  this->transfer_byte(data);
  this->disable();
  return true;
}

bool BMP581SPIComponent::bmp_read_bytes(uint8_t a_register, uint8_t *data, size_t len) {
  this->enable();
  this->transfer_byte(a_register | BMP_SPI_READ);
  this->read_array(data, len);
  this->disable();
  return true;
}

bool BMP581SPIComponent::bmp_write_bytes(uint8_t a_register, uint8_t *data, size_t len) {
  this->enable();
  this->transfer_byte(a_register & BMP_SPI_WRITE);
  this->write_array(data, len);
  this->disable();
  return true;
}
}  // namespace esphome::bmp581_spi
