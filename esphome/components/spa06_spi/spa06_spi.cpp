#include <cstdint>
#include <cstddef>

#include "spa06_spi.h"
#include "esphome/components/spa06_base/spa06_base.h"
#include "esphome/components/spi/spi.h"

// OR (|) register with SPA06_SPI_READ for read.
inline constexpr uint8_t SPA06_SPI_READ = 0x80;

// AND (&) register with SPA06_SPI_WRITE for write.
inline constexpr uint8_t SPA06_SPI_WRITE = 0x7F;

namespace esphome::spa06_spi {

static const char *const TAG = "spa06_spi";

void SPA06SPIComponent::dump_config() {
  SPA06Component::dump_config();
  LOG_SPI_DEVICE(this)
}

void SPA06SPIComponent::setup() {
  this->spi_setup();
  SPA06Component::setup();
}

void SPA06SPIComponent::protocol_reset() {
  // Forces the device into SPI mode using a dummy read
  uint8_t dummy_read = 0;
  this->spa_read_byte(spa06_base::SPA06_ID, &dummy_read);
}

// In SPI mode, only 7 bits of the register addresses are used; the MSB of register address
// is not used and replaced by a read/write bit (RW = ‘0’ for write and RW = ‘1’ for read).
// Example: address 0xF7 is accessed by using SPI register address 0x77. For write access,
// the byte 0x77 is transferred, for read access, the byte 0xF7 is transferred.
// The expressions SPA06_SPI_READ (| with register) and SPA06_SPI_WRITE (& with register)
// are defined for readability.

bool SPA06SPIComponent::spa_read_byte(uint8_t a_register, uint8_t *data) {
  this->enable();
  this->transfer_byte(a_register | SPA06_SPI_READ);
  *data = this->transfer_byte(0);
  this->disable();
  return true;
}

bool SPA06SPIComponent::spa_write_byte(uint8_t a_register, uint8_t data) {
  this->enable();
  this->transfer_byte(a_register & SPA06_SPI_WRITE);
  this->transfer_byte(data);
  this->disable();
  return true;
}

bool SPA06SPIComponent::spa_read_bytes(uint8_t a_register, uint8_t *data, size_t len) {
  this->enable();
  this->transfer_byte(a_register | SPA06_SPI_READ);
  this->read_array(data, len);
  this->disable();
  return true;
}

bool SPA06SPIComponent::spa_write_bytes(uint8_t a_register, uint8_t *data, size_t len) {
  this->enable();
  this->transfer_byte(a_register & SPA06_SPI_WRITE);
  this->write_array(data, len);
  this->disable();
  return true;
}
}  // namespace esphome::spa06_spi
