#include <cstdint>
#include <cstddef>

#include "bmp581_spi.h"
#include <esphome/components/bmp581_base/bmp581_base.h>

namespace esphome {
namespace bmp581_spi {

static const uint8_t DUMMY_SPI_DATA[2] = {0x00, 0x00};
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

// In order for the BMP581 to enter SPI mode, we need to set the SPI pin low for
// at least 16 clock cycles. We do this by doing a dummy read before sending the
// initial soft reset (in case it's in I2C/I3C mode), then perform the soft
// reset, then do a dummy read again.
bool BMP581SPIComponent::reset_() {
  this->write_array(DUMMY_SPI_DATA, 2);
  delay(3);
  if (!this->bmp_write_byte(bmp581_base::BMP581_COMMAND, bmp581_base::RESET_COMMAND)) {
    ESP_LOGE(TAG, "Failed to write reset command");

    return false;
  }

  // t_{soft_res} = 2ms (page 11 of datasheet); time it takes to enter standby mode
  //  - round up to 3 ms
  delay(3);

  // read interrupt status register
  if (!this->bmp_read_byte(bmp581_base::BMP581_INT_STATUS, &this->int_status_.reg)) {
    ESP_LOGE(TAG, "Failed to read interrupt status register");

    return false;
  }

  // Power-On-Reboot bit is asserted if sensor successfully reset
  return this->int_status_.bit.por;
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

}  // namespace bmp581_spi
}  // namespace esphome
