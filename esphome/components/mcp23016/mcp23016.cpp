#include "mcp23016.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <cstdio>

namespace esphome::mcp23016 {

ESPHOME_LOG_TAG(TAG, "mcp23016");

void MCP23016::setup() {
  uint16_t iocon;
  // MCP23016 registers operate as paired 16-bit registers. Addressing the
  // odd register (e.g. IOCON1) reads/writes that register first, then wraps
  // to the even register (IOCON0) in the same pair. Starting from the odd
  // address gives the correct byte order for 1 << pin mapping:
  // high byte = port 1 (pins 8-15), low byte = port 0 (pins 0-7).
  if (!this->read_reg_(MCP23016_IOCON1, &iocon)) {
    this->mark_failed();
    return;
  }

  // Read current output register state
  this->read_reg_(MCP23016_OLAT1, &this->olat_);

  // all pins input
  this->write_reg_(MCP23016_IODIR1, 0xFFFF);

  this->setup_interrupt_pin_();
}

bool MCP23016::digital_read_hw(uint8_t pin) { return this->read_reg_(MCP23016_GP1, &this->input_mask_); }

bool MCP23016::digital_read_cache(uint8_t pin) { return this->input_mask_ & (1 << pin); }
void MCP23016::digital_write_hw(uint8_t pin, bool value) { this->update_reg_(pin, value, MCP23016_OLAT1); }
void MCP23016::pin_mode(uint8_t pin, gpio::Flags flags) {
  if (flags == gpio::FLAG_INPUT) {
    this->update_reg_(pin, true, MCP23016_IODIR1);
    this->enable_polling_loop_();
  } else if (flags == gpio::FLAG_OUTPUT) {
    this->update_reg_(pin, false, MCP23016_IODIR1);
  }
}
bool MCP23016::read_reg_(uint8_t reg, uint16_t *value) {
  if (this->is_failed())
    return false;

  return this->read_byte_16(reg, value);
}
bool MCP23016::write_reg_(uint8_t reg, uint16_t value) {
  if (this->is_failed())
    return false;

  return this->write_byte_16(reg, value);
}
void MCP23016::update_reg_(uint8_t pin, bool pin_value, uint8_t reg_addr) {
  uint16_t reg_value = 0;

  if (reg_addr == MCP23016_OLAT1) {
    reg_value = this->olat_;
  } else {
    this->read_reg_(reg_addr, &reg_value);
  }

  assign_bits(reg_value, 1 << pin, pin_value);

  this->write_reg_(reg_addr, reg_value);

  if (reg_addr == MCP23016_OLAT1) {
    this->olat_ = reg_value;
  }
}

}  // namespace esphome::mcp23016

ESPHOME_EXPANDER_PIN_SUMMARY(esphome::mcp23016::MCP23016, "%u via MCP23016")
