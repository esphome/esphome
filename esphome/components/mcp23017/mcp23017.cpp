#include "mcp23017.h"
#include "esphome/core/log.h"

namespace esphome {
namespace mcp23017 {

static const char *TAG = "mcp23017";

void MCP23017::setup() {
  ESP_LOGCONFIG(TAG, "Setting up MCP23017...");
  uint8_t iocon;
  if (!this->read_reg_(MCP23017_IOCONA, &iocon)) {
    this->mark_failed();
    return;
  }

  // all pins input
  this->write_reg_(MCP23017_IODIRA, 0xFF);
  this->write_reg_(MCP23017_IODIRB, 0xFF);
}
bool MCP23017::digital_read(uint8_t pin) {
  uint8_t bit = pin % 8;
  uint8_t reg_addr = pin < 8 ? MCP23017_GPIOA : MCP23017_GPIOB;
  uint8_t value = 0;
  this->read_reg_(reg_addr, &value);
  return value & (1 << bit);
}
void MCP23017::digital_write(uint8_t pin, bool value) {
  uint8_t reg_addr = pin < 8 ? MCP23017_OLATA : MCP23017_OLATB;
  this->update_reg_(pin, value, reg_addr);
}
void MCP23017::pin_mode(uint8_t pin, uint8_t mode) {
  uint8_t iodir = pin < 8 ? MCP23017_IODIRA : MCP23017_IODIRB;
  uint8_t gppu = pin < 8 ? MCP23017_GPPUA : MCP23017_GPPUB;
  switch (mode) {
    case MCP23017_INPUT:
      this->update_reg_(pin, true, iodir);
      break;
    case MCP23017_INPUT_PULLUP:
      this->update_reg_(pin, true, iodir);
      this->update_reg_(pin, true, gppu);
      break;
    case MCP23017_OUTPUT:
      this->update_reg_(pin, false, iodir);
      break;
    default:
      break;
  }
}
float MCP23017::get_setup_priority() const { return setup_priority::IO; }
bool MCP23017::read_reg_(uint8_t reg, uint8_t *value) {
  if (this->is_failed())
    return false;

  return this->read_byte(reg, value);
}
bool MCP23017::write_reg_(uint8_t reg, uint8_t value) {
  if (this->is_failed())
    return false;

  return this->write_byte(reg, value);
}
void MCP23017::update_reg_(uint8_t pin, bool pin_value, uint8_t reg_addr) {
  uint8_t bit = pin % 8;
  uint8_t reg_value = 0;
  if (reg_addr == MCP23017_OLATA) {
    reg_value = this->olat_a_;
  } else if (reg_addr == MCP23017_OLATB) {
    reg_value = this->olat_b_;
  } else {
    this->read_reg_(reg_addr, &reg_value);
  }

  if (pin_value)
    reg_value |= 1 << bit;
  else
    reg_value &= ~(1 << bit);

  this->write_reg_(reg_addr, reg_value);

  if (reg_addr == MCP23017_OLATA) {
    this->olat_a_ = reg_value;
  } else if (reg_addr == MCP23017_OLATB) {
    this->olat_b_ = reg_value;
  }
}

MCP23017GPIOPin::MCP23017GPIOPin(MCP23017 *parent, uint8_t pin, uint8_t mode, bool inverted)
    : GPIOPin(pin, mode, inverted), parent_(parent) {}
void MCP23017GPIOPin::setup() { this->pin_mode(this->mode_); }
void MCP23017GPIOPin::pin_mode(uint8_t mode) { this->parent_->pin_mode(this->pin_, mode); }
bool MCP23017GPIOPin::digital_read() { return this->parent_->digital_read(this->pin_) != this->inverted_; }
void MCP23017GPIOPin::digital_write(bool value) { this->parent_->digital_write(this->pin_, value != this->inverted_); }

}  // namespace mcp23017
}  // namespace esphome
