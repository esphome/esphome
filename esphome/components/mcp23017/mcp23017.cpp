#include "mcp23017.h"
#include "esphome/core/log.h"

namespace esphome {
namespace mcp23017 {

static const char *const TAG = "mcp23017";

void MCP23017::setup() {
  uint8_t iocon;
  if (!this->read_reg(mcp23x17_base::MCP23X17_IOCONA, &iocon)) {
    this->mark_failed();
    return;
  }

  // Read current output register state
  this->read_reg(mcp23x17_base::MCP23X17_OLATA, &this->olat_a_);
  this->read_reg(mcp23x17_base::MCP23X17_OLATB, &this->olat_b_);

  // Configure IOCON register for interrupt operation
  uint8_t iocon_value = 0x00;
  if (this->open_drain_ints_) {
    // Enable open-drain interrupt pins, 3.3V-safe
    iocon_value |= 0x04;  // ODR bit
  }
  if (this->interrupt_pin_internal_ != nullptr) {
    // Mirror interrupts (INTA and INTB are internally connected)
    iocon_value |= 0x40;  // MIRROR bit
  }
  if (iocon_value != 0x00) {
    this->write_reg(mcp23x17_base::MCP23X17_IOCONA, iocon_value);
    this->write_reg(mcp23x17_base::MCP23X17_IOCONB, iocon_value);
  }

  // Setup interrupt pin if configured
  if (this->interrupt_pin_internal_ != nullptr) {
    this->setup_interrupt_pin(this->interrupt_pin_internal_, this);
  }
}

void MCP23017::dump_config() { ESP_LOGCONFIG(TAG, "MCP23017:"); }

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

}  // namespace mcp23017
}  // namespace esphome
