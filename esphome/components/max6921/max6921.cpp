// Datasheet: https://www.analog.com/media/en/technical-documentation/data-sheets/MAX6921-MAX6931.pdf

#include "max6921.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace max6921 {

static const char *const TAG = "max6921";

float MAX6921::get_setup_priority() const { return setup_priority::HARDWARE; }

void MAX6921::setup() {
  ESP_LOGCONFIG(TAG, "Setting up MAX6921...");
  this->spi_setup();
  this->load_pin_->setup();
  this->disable_load();
}

void MAX6921::dump_config() {
  ESP_LOGCONFIG(TAG, "MAX6921:");
  LOG_PIN("  LOAD Pin: ", this->cs_);
}

/*
 * Clocks data into MAX6921 via SPI.
 * Data must contain 3 bytes with following format:
 *   bit  | 23 | 22 | ... | 4 | 3 | 2 | 1 | 0
 *   ----------------------------------------
 *   DOUT | 19 | 18 | ... | 0 | x | x | x | x
 */
void MAX6921::write_data(uint8_t *ptr, size_t length) {
  assert(length == 3);
  this->disable_load();                   // set LOAD to low
  this->transfer_array(ptr, length);
  this->enable_load();                    // set LOAD to high to update output latch
}

}  // namespace max6921
}  // namespace esphome
