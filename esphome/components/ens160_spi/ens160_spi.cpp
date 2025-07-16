#include <cstdint>
#include <cstddef>

#include "ens160_spi.h"
#include <esphome/components/ens160_base/ens160_base.h>

namespace esphome {
namespace ens160_spi {

static const char *const TAG = "ens160_spi.sensor";

inline uint8_t reg_read(uint8_t reg) { return (reg << 1) | 0x01; }

inline uint8_t reg_write(uint8_t reg) { return (reg << 1) & 0xFE; }

void ENS160SPIComponent::setup() {
  this->spi_setup();
  ENS160Component::setup();
};

void ENS160SPIComponent::dump_config() {
  ENS160Component::dump_config();
  LOG_PIN("  CS Pin: ", this->cs_);
}

bool ENS160SPIComponent::read_byte(uint8_t a_register, uint8_t *data) {
  this->enable();
  this->transfer_byte(reg_read(a_register));
  *data = this->transfer_byte(0);
  this->disable();
  return true;
}

bool ENS160SPIComponent::write_byte(uint8_t a_register, uint8_t data) {
  this->enable();
  this->transfer_byte(reg_write(a_register));
  this->transfer_byte(data);
  this->disable();
  return true;
}

bool ENS160SPIComponent::read_bytes(uint8_t a_register, uint8_t *data, size_t len) {
  this->enable();
  this->transfer_byte(reg_read(a_register));
  this->read_array(data, len);
  this->disable();
  return true;
}

bool ENS160SPIComponent::write_bytes(uint8_t a_register, uint8_t *data, size_t len) {
  this->enable();
  this->transfer_byte(reg_write(a_register));
  this->transfer_array(data, len);
  this->disable();
  return true;
}

}  // namespace ens160_spi
}  // namespace esphome
