#include "mcp23017.h"
#include "esphome/core/log.h"

namespace esphome::mcp23017 {

static const char *const TAG = "mcp23017";

static constexpr uint8_t IOCON_MIRROR = 0x40;  // Mirror INTA/INTB pins
static constexpr uint8_t IOCON_ODR = 0x04;     // Open-drain output for INT pin

void MCP23017::setup() {
  uint8_t iocon;
  if (!this->read_reg(mcp23x17_base::MCP23X17_IOCONA, &iocon)) {
    this->mark_failed();
    return;
  }

  // Read current output register state
  this->read_reg(mcp23x17_base::MCP23X17_OLATA, &this->olat_a_);
  this->read_reg(mcp23x17_base::MCP23X17_OLATB, &this->olat_b_);

  uint8_t iocon_flags = 0;
  if (this->open_drain_ints_) {
    iocon_flags |= IOCON_ODR;
  }
  if (this->interrupt_pin_ != nullptr) {
    // Mirror INTA/INTB so either pin fires for changes on any port
    iocon_flags |= IOCON_MIRROR;
  }
  if (iocon_flags != 0) {
    this->write_reg(mcp23x17_base::MCP23X17_IOCONA, iocon | iocon_flags);
    this->write_reg(mcp23x17_base::MCP23X17_IOCONB, iocon | iocon_flags);
  }

  this->setup_interrupt_pin_();
}

void MCP23017::dump_config() {
  ESP_LOGCONFIG(TAG, "MCP23017:");
  LOG_PIN("  Interrupt Pin: ", this->interrupt_pin_);
}

bool MCP23017::read_reg(uint8_t reg, uint8_t *value) {
  if (this->is_failed())
    return false;

  return this->read_byte(reg, value);
}
bool MCP23017::write_reg(uint8_t reg, uint8_t value) {
  if (this->is_failed())
    return false;

  return this->write_byte(reg, value);
}

}  // namespace esphome::mcp23017
