#include "mcp23008.h"
#include "esphome/core/log.h"

namespace esphome::mcp23008 {

static const char *const TAG = "mcp23008";

static constexpr uint8_t IOCON_ODR = 0x04;  // Open-drain output for INT pin

void MCP23008::setup() {
  uint8_t iocon;
  if (!this->read_reg(mcp23x08_base::MCP23X08_IOCON, &iocon)) {
    this->mark_failed();
    return;
  }

  // Read current output register state
  this->read_reg(mcp23x08_base::MCP23X08_OLAT, &this->olat_);

  if (this->open_drain_ints_) {
    // enable open-drain interrupt pins, 3.3V-safe
    this->write_reg(mcp23x08_base::MCP23X08_IOCON, iocon | IOCON_ODR);
  }

  this->setup_interrupt_pin_();
}

void MCP23008::dump_config() {
  ESP_LOGCONFIG(TAG, "MCP23008:");
  LOG_PIN("  Interrupt Pin: ", this->interrupt_pin_);
}

bool MCP23008::read_reg(uint8_t reg, uint8_t *value) {
  if (this->is_failed())
    return false;

  return this->read_byte(reg, value);
}

bool MCP23008::write_reg(uint8_t reg, uint8_t value) {
  if (this->is_failed())
    return false;

  return this->write_byte(reg, value);
}

}  // namespace esphome::mcp23008
