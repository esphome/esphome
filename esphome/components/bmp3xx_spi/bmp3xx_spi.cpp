#include "bmp3xx_spi.h"
#include <cinttypes>

int set_bit(uint8_t num, int position) {
  int mask = 1 << position;
  return num | mask;
}

int clear_bit(uint8_t num, int position) {
  int mask = 1 << position;
  return num & ~mask;
}

namespace esphome {
namespace bmp3xx_spi {

static const char *const TAG = "bmp3xx_spi.sensor";

void BMP3XXSPIComponent::setup() {
  this->spi_setup();
  BMP3XXComponent::setup();
}

bool BMP3XXSPIComponent::read_byte(uint8_t a_register, uint8_t *data) {
  this->enable();
  // cause: *data = this->delegate_->transfer(tmp) doesnt work
  this->delegate_->transfer(set_bit(a_register, 7));
  *data = this->delegate_->transfer(0);
  this->disable();
  return true;
}

bool BMP3XXSPIComponent::write_byte(uint8_t a_register, uint8_t data) {
  this->enable();
  this->delegate_->transfer(clear_bit(a_register, 7));
  this->delegate_->transfer(data);
  this->disable();
  return true;
}

bool BMP3XXSPIComponent::read_bytes(uint8_t a_register, uint8_t *data, size_t len) {
  this->enable();
  this->delegate_->transfer(set_bit(a_register, 7));
  this->delegate_->read_array(data, len);
  this->disable();
  return true;
}

bool BMP3XXSPIComponent::write_bytes(uint8_t a_register, uint8_t *data, size_t len) {
  this->enable();
  this->delegate_->transfer(clear_bit(a_register, 7));
  this->delegate_->transfer(data, len);
  this->disable();
  return true;
}

}  // namespace bmp3xx_spi
}  // namespace esphome
