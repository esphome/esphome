#include "mcp23017.h"
#include "esphome/core/log.h"

namespace esphome::mcp23017 {

ESPHOME_LOG_TAG(TAG, "mcp23017");

static constexpr uint8_t IOCON_MIRROR = 0x40;  // Mirror INTA/INTB pins
static constexpr uint8_t IOCON_ODR = 0x04;     // Open-drain output for INT pin
static constexpr uint8_t IOCON_MASK = IOCON_ODR | IOCON_MIRROR;

void MCP23017::setup() {
  // Mirror INTA/INTB so either pin fires for changes on any port
  const uint8_t iocon = (this->open_drain_ints_ ? IOCON_ODR : 0) | (this->interrupt_pin_ != nullptr ? IOCON_MIRROR : 0);
  // In BANK=0 IOCONB is the same register as IOCONA
  if (!this->setup_check(this->update_register(mcp23x17_base::MCP23X17_IOCONA, IOCON_MASK, iocon)))
    return;

  // Read current output register state
  this->read_reg(mcp23x17_base::MCP23X17_OLATA, &this->olat_a_);
  this->read_reg(mcp23x17_base::MCP23X17_OLATB, &this->olat_b_);

  // Reset IPOL to 0x00: ESPHome handles 'inverted' in software.
  this->write_reg(mcp23x17_base::MCP23X17_IPOLA, 0x00);
  this->write_reg(mcp23x17_base::MCP23X17_IPOLB, 0x00);

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
