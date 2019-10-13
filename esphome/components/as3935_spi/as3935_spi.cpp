#include "as3935_spi.h"
#include "esphome/core/log.h"

namespace esphome {
namespace as3935_spi {

static const char *TAG = "as3935_spi";

void SPIAS3935Component::setup() {
  ESP_LOGI(TAG, "SPIAS3935Component setup started!");
  this->spi_setup();
  ESP_LOGI(TAG, "SPI setup finished!");
  AS3935Component::setup();
}

void SPIAS3935Component::dump_config() {
  AS3935Component::dump_config();
  LOG_PIN("  CS Pin: ", this->cs_);
}

void SPIAS3935Component::write_register(uint8_t reg, uint8_t mask, uint8_t bits, uint8_t start_pos) {
  uint8_t write_reg = this->read_register(reg);

  write_reg &= (~mask);
  write_reg |= (bits << start_pos);

  this->enable();
  this->write_byte(reg);
  this->write_byte(write_reg);
  this->disable();
}

uint8_t SPIAS3935Component::read_register(uint8_t reg) {
  uint8_t value = 0;
  this->enable();
  this->write_byte(reg |= SPI_READ_M);
  value = this->read_byte();
  // According to datsheet, the chip select must be written HIGH, LOW, HIGH
  // to correctly end the READ command.
  this->cs_->digital_write(true);
  this->cs_->digital_write(false);
  this->disable();
  ESP_LOGV(TAG, "read_register_: %d", value);
  return value;
}

}  // namespace as3935_spi
}  // namespace esphome
