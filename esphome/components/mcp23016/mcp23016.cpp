#include "mcp23016.h"
#include "esphome/core/log.h"
#include <cstdio>

namespace esphome {
namespace mcp23016 {

static const char *const TAG = "mcp23016";

void MCP23016::setup() {
  ESP_LOGCONFIG(TAG, "Setting up MCP23016...");
  uint8_t iocon;
  if (!this->read_reg_(MCP23016_IOCON0, &iocon)) {
    this->mark_failed();
    return;
  }

  // Read current output register state
  this->read_reg_(MCP23016_OLAT0, &this->olat_0_);
  this->read_reg_(MCP23016_OLAT1, &this->olat_1_);

  // all pins input
  this->write_reg_(MCP23016_IODIR0, 0xFF);
  this->write_reg_(MCP23016_IODIR1, 0xFF);
}
bool MCP23016::digital_read(uint8_t pin) {
  uint8_t bit = pin % 8;
  uint8_t reg_addr = pin < 8 ? MCP23016_GP0 : MCP23016_GP1;
  uint8_t value = 0;
  this->read_reg_(reg_addr, &value);
  return value & (1 << bit);
}
void MCP23016::digital_write(uint8_t pin, bool value) {
  uint8_t reg_addr = pin < 8 ? MCP23016_OLAT0 : MCP23016_OLAT1;
  this->update_reg_(pin, value, reg_addr);
}
void MCP23016::pin_mode(uint8_t pin, gpio::Flags flags) {
  uint8_t iodir = pin < 8 ? MCP23016_IODIR0 : MCP23016_IODIR1;
  if (flags == gpio::FLAG_INPUT) {
    this->update_reg_(pin, true, iodir);
  } else if (flags == gpio::FLAG_OUTPUT) {
    this->update_reg_(pin, false, iodir);
  }
}
float MCP23016::get_setup_priority() const { return setup_priority::HARDWARE; }
bool MCP23016::read_reg_(uint8_t reg, uint8_t *value) {
  if (this->is_failed())
    return false;

  return this->read_byte(reg, value);
}
bool MCP23016::write_reg_(uint8_t reg, uint8_t value) {
  if (this->is_failed())
    return false;

  return this->write_byte(reg, value);
}
void MCP23016::update_reg_(uint8_t pin, bool pin_value, uint8_t reg_addr) {
  uint8_t bit = pin % 8;
  uint8_t reg_value = 0;
  if (reg_addr == MCP23016_OLAT0) {
    reg_value = this->olat_0_;
  } else if (reg_addr == MCP23016_OLAT1) {
    reg_value = this->olat_1_;
  } else {
    this->read_reg_(reg_addr, &reg_value);
  }

  if (pin_value) {
    reg_value |= 1 << bit;
  } else {
    reg_value &= ~(1 << bit);
  }

  this->write_reg_(reg_addr, reg_value);

  if (reg_addr == MCP23016_OLAT0) {
    this->olat_0_ = reg_value;
  } else if (reg_addr == MCP23016_OLAT1) {
    this->olat_1_ = reg_value;
  }
}

void MCP23016GPIOPin::setup() { pin_mode(flags_); }
void MCP23016GPIOPin::pin_mode(gpio::Flags flags) { this->parent_->pin_mode(this->pin_, flags); }
bool MCP23016GPIOPin::digital_read() { return this->parent_->digital_read(this->pin_) != this->inverted_; }
void MCP23016GPIOPin::digital_write(bool value) { this->parent_->digital_write(this->pin_, value != this->inverted_); }
std::string MCP23016GPIOPin::dump_summary() const {
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "%u via MCP23016", pin_);
  return buffer;
}

}  // namespace mcp23016
}  // namespace esphome
