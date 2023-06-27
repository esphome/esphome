#include "mcp23017.h"
#include "esphome/core/log.h"

namespace esphome {
namespace mcp23017 {

static const char *const TAG = "mcp23017";

void MCP23017::setup() {
  ESP_LOGCONFIG(TAG, "Setting up MCP23017...");
  uint8_t iocon;
  if (!this->read_reg(mcp23x17_base::MCP23X17_IOCONA, &iocon)) {
    this->mark_failed();
    return;
  }

  // Read current output register state
  this->read_reg(mcp23x17_base::MCP23X17_OLATA, &this->olat_a_);
  this->read_reg(mcp23x17_base::MCP23X17_OLATB, &this->olat_b_);

  if (this->open_drain_ints_) {
    // enable open-drain interrupt pins, 3.3V-safe
    this->write_reg(mcp23x17_base::MCP23X17_IOCONA, 0x04);
    this->write_reg(mcp23x17_base::MCP23X17_IOCONB, 0x04);
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
