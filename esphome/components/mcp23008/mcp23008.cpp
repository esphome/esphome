#include "mcp23008.h"
#include "esphome/core/log.h"

namespace esphome {
namespace mcp23008 {

static const char *const TAG = "mcp23008";

void MCP23008::setup() {
  ESP_LOGCONFIG(TAG, "Setting up MCP23008...");
  uint8_t iocon;
  if (!this->read_reg(mcp23x08_base::MCP23X08_IOCON, &iocon)) {
    this->mark_failed();
    return;
  }

  if (this->open_drain_ints_) {
    // enable open-drain interrupt pins, 3.3V-safe
    this->write_reg(mcp23x08_base::MCP23X08_IOCON, 0x04);
  }
}

void MCP23008::dump_config() { ESP_LOGCONFIG(TAG, "MCP23008:"); }

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

}  // namespace mcp23008
}  // namespace esphome
