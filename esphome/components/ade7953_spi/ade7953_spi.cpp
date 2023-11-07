#include "ade7953_spi.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace ade7953_spi {

static const char *const TAG = "ade7953";

void AdE7953Spi::setup() {
  this->spi_setup();
  ade7953_base::ADE7953::setup();
}

void AdE7953Spi::dump_config() {
  ESP_LOGCONFIG(TAG, "ADE7953_spi:");
  LOG_PIN("  CS Pin: ", this->cs_);
  ade7953_base::ADE7953::dump_config();
}

bool AdE7953Spi::ade_write_8(uint16_t reg, uint8_t value) {
  this->enable();
  this->write_byte16(reg);
  this->transfer_byte(0);
  this->transfer_byte(value);
  this->disable();
  return false;
}

bool AdE7953Spi::ade_write_16(uint16_t reg, uint16_t value) {
  this->enable();
  this->write_byte16(reg);
  this->transfer_byte(0);
  this->write_byte16(value);
  this->disable();
  return false;
}

bool AdE7953Spi::ade_write_32(uint16_t reg, uint32_t value) {
  this->enable();
  this->write_byte16(reg);
  this->transfer_byte(0);
  this->write_byte16(value >> 16);
  this->write_byte16(value & 0xFFFF);
  this->disable();
  return false;
}

bool AdE7953Spi::ade_read_8(uint16_t reg, uint8_t *value) {
  this->enable();
  this->write_byte16(reg);
  this->transfer_byte(0x80);
  *value = this->read_byte();
  this->disable();
  return false;
}

bool AdE7953Spi::ade_read_16(uint16_t reg, uint16_t *value) {
  this->enable();
  this->write_byte16(reg);
  this->transfer_byte(0x80);
  uint8_t recv[2];
  this->read_array(recv, 4);
  *value = encode_uint16(recv[0], recv[1]);
  this->disable();
  return false;
}

bool AdE7953Spi::ade_read_32(uint16_t reg, uint32_t *value) {
  this->enable();
  this->write_byte16(reg);
  this->transfer_byte(0x80);
  uint8_t recv[4];
  this->read_array(recv, 4);
  *value = encode_uint32(recv[0], recv[1], recv[2], recv[3]);
  this->disable();
  return false;
}

}  // namespace ade7953_spi
}  // namespace esphome
