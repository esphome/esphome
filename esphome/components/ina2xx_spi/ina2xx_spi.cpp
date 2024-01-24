#include "ina2xx_spi.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ina2xx_spi {

static const char *const TAG = "ina2xx_spi";

void INA2XXSPI::setup() {
  this->spi_setup();
  INA2XX::setup();
}

void INA2XXSPI::dump_config() {
  INA2XX::dump_config();
  LOG_PIN("  CS Pin: ", this->cs_);
}

bool INA2XXSPI::read_ina_register(uint8_t reg, uint8_t *data, size_t len) {
  reg = (reg << 2);  // top 6 bits
  reg |= 0x01;       // read
  this->enable();
  this->write_byte(reg);
  this->read_array(data, len);
  this->disable();
  return true;
}

bool INA2XXSPI::write_ina_register(uint8_t reg, const uint8_t *data, size_t len) {
  reg = (reg << 2);  // top 6 bits
  this->enable();
  this->write_byte(reg);
  this->write_array(data, len);
  this->disable();
  return true;
}
}  // namespace ina2xx_spi
}  // namespace esphome
