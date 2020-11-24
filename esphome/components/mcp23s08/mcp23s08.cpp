#include "mcp23s08.h"
#include "esphome/core/log.h"

namespace esphome {
namespace mcp23s08 {

static const char *TAG = "mcp23s08";

void MCP23S08::set_device_address(uint8_t device_addr) {
  if (device_addr != 0) {
    this->device_opcode_ |= ((device_addr & 0x03) << 1);
  }
}

void MCP23S08::setup() {
  ESP_LOGCONFIG(TAG, "Setting up MCP23S08...");
  this->spi_setup();
  this->enable();

  this->transfer_byte(MCP23S08_IODIR);
  this->transfer_byte(0xFF);
  for (uint8_t i = 0; i < MCP23S08_OLAT; i++) {
    this->transfer_byte(0x00);
  }
  this->disable();
}

void MCP23S08::dump_config() {
  ESP_LOGCONFIG(TAG, "MCP23S08:");
  LOG_PIN("  CS Pin: ", this->cs_);
}

float MCP23S08::get_setup_priority() const { return setup_priority::HARDWARE; }

bool MCP23S08::digital_read(uint8_t pin) {
  if (pin > 7) {
    return false;
  }
  uint8_t bit = pin % 8;
  uint8_t reg_addr = MCP23S08_GPIO;
  uint8_t value = 0;
  this->read_reg(reg_addr, &value);
  return value & (1 << bit);
}

void MCP23S08::digital_write(uint8_t pin, bool value) {
  if (pin > 7) {
    return;
  }
  uint8_t reg_addr = MCP23S08_OLAT;
  this->update_reg(pin, value, reg_addr);
}

void MCP23S08::pin_mode(uint8_t pin, uint8_t mode) {
  uint8_t iodir = MCP23S08_IODIR;
  uint8_t gppu = MCP23S08_GPPU;
  switch (mode) {
    case MCP23S08_INPUT:
      this->update_reg(pin, true, iodir);
      break;
    case MCP23S08_INPUT_PULLUP:
      this->update_reg(pin, true, iodir);
      this->update_reg(pin, true, gppu);
      break;
    case MCP23S08_OUTPUT:
      this->update_reg(pin, false, iodir);
      break;
    default:
      break;
  }
}

void MCP23S08::update_reg(uint8_t pin, bool pin_value, uint8_t reg_addr) {
  uint8_t bit = pin % 8;
  uint8_t reg_value = 0;
  if (reg_addr == MCP23S08_OLAT) {
    reg_value = this->olat_;
  } else {
    this->read_reg(reg_addr, &reg_value);
  }

  if (pin_value)
    reg_value |= 1 << bit;
  else
    reg_value &= ~(1 << bit);

  this->write_reg(reg_addr, reg_value);

  if (reg_addr == MCP23S08_OLAT) {
    this->olat_ = reg_value;
  }
}

bool MCP23S08::write_reg(uint8_t reg, uint8_t value) {
  this->enable();
  this->transfer_byte(this->device_opcode_);
  this->transfer_byte(reg);
  this->transfer_byte(value);
  this->disable();
  return true;
}

bool MCP23S08::read_reg(uint8_t reg, uint8_t *value) {
  uint8_t data;
  this->enable();
  this->transfer_byte(this->device_opcode_ | 1);
  this->transfer_byte(reg);
  *value = this->transfer_byte(0);
  this->disable();
  return true;
}

MCP23S08GPIOPin::MCP23S08GPIOPin(MCP23S08 *parent, uint8_t pin, uint8_t mode, bool inverted)
    : GPIOPin(pin, mode, inverted), parent_(parent) {}
void MCP23S08GPIOPin::setup() { this->pin_mode(this->mode_); }
void MCP23S08GPIOPin::pin_mode(uint8_t mode) { this->parent_->pin_mode(this->pin_, mode); }
bool MCP23S08GPIOPin::digital_read() { return this->parent_->digital_read(this->pin_) != this->inverted_; }
void MCP23S08GPIOPin::digital_write(bool value) { this->parent_->digital_write(this->pin_, value != this->inverted_); }

}  // namespace mcp23s08
}  // namespace esphome
