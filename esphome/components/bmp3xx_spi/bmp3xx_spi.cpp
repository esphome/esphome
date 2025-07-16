#include "bmp3xx_spi.h"
#include <cinttypes>

namespace esphome {
namespace bmp3xx_spi {

static const char *const TAG = "bmp3xx_spi.sensor";

uint8_t set_bit(uint8_t num, int position) {
  int mask = 1 << position;
  return num | mask;
}

uint8_t clear_bit(uint8_t num, int position) {
  int mask = 1 << position;
  return num & ~mask;
}

void BMP3XXSPIComponent::setup() {
  this->spi_setup();
  BMP3XXComponent::setup();
}

bool BMP3XXSPIComponent::read_byte(uint8_t a_register, uint8_t *data) {
  this->enable();
  this->transfer_byte(set_bit(a_register, 7));
  *data = this->transfer_byte(0);
  this->disable();
  return true;
}

bool BMP3XXSPIComponent::write_byte(uint8_t a_register, uint8_t data) {
  this->enable();
  this->transfer_byte(clear_bit(a_register, 7));
  this->transfer_byte(data);
  this->disable();
  return true;
}

bool BMP3XXSPIComponent::read_bytes(uint8_t a_register, uint8_t *data, size_t len) {
  this->enable();
  this->transfer_byte(set_bit(a_register, 7));
  this->read_array(data, len);
  this->disable();
  return true;
}

bool BMP3XXSPIComponent::write_bytes(uint8_t a_register, uint8_t *data, size_t len) {
  this->enable();
  this->transfer_byte(clear_bit(a_register, 7));
  this->transfer_array(data, len);
  this->disable();
  return true;
}

}  // namespace bmp3xx_spi
}  // namespace esphome
