#include "st25r_spi.h"
#include "esphome/core/log.h"

namespace esphome::st25r_spi {

static const char *const TAG = "st25r_spi";

void ST25RSpi::setup() {
  this->spi_setup();
  st25r::ST25R::setup();
}

void ST25RSpi::dump_config() {
  st25r::ST25R::dump_config();
  LOG_PIN("  CS Pin: ", this->cs_);
}

uint8_t ST25RSpi::read_register(uint8_t reg) {
  this->enable();
  if (reg & 0x40) {
    this->write_byte(0xFB);  // Space B prefix
  }
  this->write_byte(0x40 | (reg & 0x3F));
  uint8_t value = this->read_byte();
  this->disable();
  return value;
}

void ST25RSpi::write_register(uint8_t reg, uint8_t value) {
  this->enable();
  if (reg & 0x40) {
    this->write_byte(0xFB);  // Space B prefix
  }
  this->write_byte(0x00 | (reg & 0x3F));
  this->write_byte(value);
  this->disable();
}

void ST25RSpi::write_command(uint8_t command) {
  this->enable();
  this->write_byte(command);
  this->disable();
}

void ST25RSpi::write_fifo(const uint8_t *data, size_t len) {
  this->enable();
  this->write_byte(0x80);
  for (size_t i = 0; i < len; i++) {
    this->write_byte(data[i]);
  }
  this->disable();
}

void ST25RSpi::read_fifo(uint8_t *data, size_t len) {
  this->enable();
  this->write_byte(0x9F);
  for (size_t i = 0; i < len; i++) {
    data[i] = this->read_byte();
  }
  this->disable();
}

}  // namespace esphome::st25r_spi
