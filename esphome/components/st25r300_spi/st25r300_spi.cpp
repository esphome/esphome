#include "st25r300_spi.h"
#include "esphome/core/log.h"

namespace esphome {
namespace st25r300_spi {

static const char *const TAG = "st25r300_spi";

void ST25R300Spi::setup() {
  ESP_LOGI(TAG, "Setting up ST25R300 SPI device...");
  this->spi_setup();
  st25r300::ST25R300::setup();
}

void ST25R300Spi::dump_config() {
  st25r300::ST25R300::dump_config();
  LOG_PIN("  CS Pin: ", this->cs_);
}

uint8_t ST25R300Spi::read_register(uint8_t reg) {
  this->enable();
  this->write_byte(0x80 | (reg & 0x7F));  // bit7=1 → read
  uint8_t value = this->read_byte();
  this->disable();
  return value;
}

void ST25R300Spi::write_register(uint8_t reg, uint8_t value) {
  this->enable();
  this->write_byte(reg & 0x7F);  // bit7=0 → write
  this->write_byte(value);
  this->disable();
}

void ST25R300Spi::write_command(uint8_t command) {
  this->enable();
  this->write_byte(command);
  this->disable();
}

void ST25R300Spi::write_fifo(const uint8_t *data, size_t len) {
  this->enable();
  this->write_byte(0x5F);  // FIFO write access byte
  for (size_t i = 0; i < len; i++) {
    this->write_byte(data[i]);
  }
  this->disable();
}

void ST25R300Spi::read_fifo(uint8_t *data, size_t len) {
  this->enable();
  this->write_byte(0xDF);  // FIFO read access byte (0x80 | 0x5F)
  for (size_t i = 0; i < len; i++) {
    data[i] = this->read_byte();
  }
  this->disable();
}

}  // namespace st25r300_spi
}  // namespace esphome
