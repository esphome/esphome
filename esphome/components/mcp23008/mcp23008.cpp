#include "mcp23008.h"
#include "esphome/core/log.h"

namespace esphome {
namespace mcp23008 {

static const char *TAG = "mcp23008";

void MCP23008::setup() {
  ESP_LOGCONFIG(TAG, "Setting up MCP23008...");
  uint8_t iocon;
  if (!this->read_reg_(MCP23008_IOCON, &iocon)) {
    this->mark_failed();
    return;
  }

  // all pins input
  this->write_reg_(MCP23008_IODIR, 0xFF);
}
bool MCP23008::digital_read(uint8_t pin) {
  uint8_t bit = pin % 8;
  uint8_t reg_addr = MCP23008_GPIO;
  uint8_t value = 0;
  this->read_reg_(reg_addr, &value);
  return value & (1 << bit);
}
void MCP23008::digital_write(uint8_t pin, bool value) {
  uint8_t reg_addr = MCP23008_OLAT;
  this->update_reg_(pin, value, reg_addr);
}
void MCP23008::pin_mode(uint8_t pin, uint8_t mode) {
  uint8_t iodir = MCP23008_IODIR;
  uint8_t gppu = MCP23008_GPPU;
  switch (mode) {
    case MCP23008_INPUT:
      this->update_reg_(pin, true, iodir);
      break;
    case MCP23008_INPUT_PULLUP:
      this->update_reg_(pin, true, iodir);
      this->update_reg_(pin, true, gppu);
      break;
    case MCP23008_OUTPUT:
      this->update_reg_(pin, false, iodir);
      break;
    default:
      break;
  }
}
float MCP23008::get_setup_priority() const { return setup_priority::HARDWARE; }
bool MCP23008::read_reg_(uint8_t reg, uint8_t *value) {
  if (this->is_failed())
    return false;

  return this->read_byte(reg, value);
}
bool MCP23008::write_reg_(uint8_t reg, uint8_t value) {
  if (this->is_failed())
    return false;

  return this->write_byte(reg, value);
}
void MCP23008::update_reg_(uint8_t pin, bool pin_value, uint8_t reg_addr) {
  uint8_t bit = pin % 8;
  uint8_t reg_value = 0;
  if (reg_addr == MCP23008_OLAT) {
    reg_value = this->olat_;
  } else {
    this->read_reg_(reg_addr, &reg_value);
  }

  if (pin_value)
    reg_value |= 1 << bit;
  else
    reg_value &= ~(1 << bit);

  this->write_reg_(reg_addr, reg_value);

  if (reg_addr == MCP23008_OLAT) {
    this->olat_ = reg_value;
  }
}

MCP23008GPIOPin::MCP23008GPIOPin(MCP23008 *parent, uint8_t pin, uint8_t mode, bool inverted)
    : GPIOPin(pin, mode, inverted), parent_(parent) {}
void MCP23008GPIOPin::setup() { this->pin_mode(this->mode_); }
void MCP23008GPIOPin::pin_mode(uint8_t mode) { this->parent_->pin_mode(this->pin_, mode); }
bool MCP23008GPIOPin::digital_read() { return this->parent_->digital_read(this->pin_) != this->inverted_; }
void MCP23008GPIOPin::digital_write(bool value) { this->parent_->digital_write(this->pin_, value != this->inverted_); }

}  // namespace mcp23008
}  // namespace esphome
