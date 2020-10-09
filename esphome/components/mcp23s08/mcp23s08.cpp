#include "mcp23s08.h"
#include "esphome/core/log.h"

namespace esphome {
namespace mcp23s08 {

static const char *TAG = "mcp23s08";

void MCP23S08::setup() {
  ESP_LOGCONFIG(TAG, "Setting up MCP23S08...");
  this->spi_setup();
  uint8_t iocon;
  if (!this->read_reg_(MCP23S08_IOCON, &iocon)) {
    this->mark_failed();
    return;
  }

  // all pins input
  this->write_reg_(MCP23S08_IODIR, 0xFF);
}

void MCP23S08::dump_config() {
  ESP_LOGCONFIG(TAG, "MCP23S08:");
  ESP_LOGCONFIG(TAG, "  Address: %#02x", this->address_);
  LOG_PIN("  CS Pin: ", this->cs_);
}

bool MCP23S08::digital_read(uint8_t pin) {
  uint8_t bit = pin % 8;
  uint8_t reg_addr = MCP23S08_GPIO;
  uint8_t value = 0;
  this->read_reg_(reg_addr, &value);
  return value & (1 << bit);
}
void MCP23S08::digital_write(uint8_t pin, bool value) {
  uint8_t reg_addr = MCP23S08_OLAT;


  this->update_reg_(pin, value, reg_addr);
}
void MCP23S08::pin_mode(uint8_t pin, uint8_t mode) {
  uint8_t iodir = MCP23S08_IODIR;
  uint8_t gppu = MCP23S08_GPPU;
  switch (mode) {
    case MCP23S08_INPUT:
      this->update_reg_(pin, true, iodir);
      break;
    case MCP23S08_INPUT_PULLUP:
      this->update_reg_(pin, true, iodir);
      this->update_reg_(pin, true, gppu);
      break;
    case MCP23S08_OUTPUT:
      this->update_reg_(pin, false, iodir);
      break;
    default:
      break;
  }
}

float MCP23S08::get_setup_priority() const { return setup_priority::HARDWARE; }

bool MCP23S08::read_reg_(uint8_t reg, uint8_t *value) {
  uint8_t data;
  if (this->is_failed())
    return false;
  this->enable();
  this->transfer_byte(this->address_ | 1);
  this->transfer_byte(reg);
  data = this->transfer_byte(0);
  this->disable();
  *value = data;
  return true;
}

bool MCP23S08::write_reg_(uint8_t reg, uint8_t value) {
  if (this->is_failed())
    return false;
  this->enable();
  this->transfer_byte(this->address_ | 0);
  this->transfer_byte(reg);
  this->transfer_byte(value);
  this->disable();
  return true;
}

void MCP23S08::update_reg_(uint8_t pin, bool pin_value, uint8_t reg_addr) {
  uint8_t bit = pin % 8;
  uint8_t reg_value = 0;
  if (reg_addr == MCP23S08_OLAT) {
    reg_value = this->olat_;
  } else {
    this->read_reg_(reg_addr, &reg_value);
  }

  if (pin_value)
    reg_value |= 1 << bit;
  else
    reg_value &= ~(1 << bit);

  this->write_reg_(reg_addr, reg_value);

  if (reg_addr == MCP23S08_OLAT) {
    this->olat_ = reg_value;
  }
}

MCP23S08GPIOPin::MCP23S08GPIOPin(MCP23S08 *parent, uint8_t pin, uint8_t mode, bool inverted)
    : GPIOPin(pin, mode, inverted), parent_(parent) {}
void MCP23S08GPIOPin::setup() { this->pin_mode(this->mode_); }
void MCP23S08GPIOPin::pin_mode(uint8_t mode) { this->parent_->pin_mode(this->pin_, mode); }
bool MCP23S08GPIOPin::digital_read() { return this->parent_->digital_read(this->pin_) != this->inverted_; }
void MCP23S08GPIOPin::digital_write(bool value) { this->parent_->digital_write(this->pin_, value != this->inverted_); }

}  // namespace mcp23s08
}  // namespace esphome
