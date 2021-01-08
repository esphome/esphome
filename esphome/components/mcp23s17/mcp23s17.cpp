#include "mcp23s17.h"
#include "esphome/core/log.h"

namespace esphome {
namespace mcp23s17 {

static const char *TAG = "mcp23s17";

void MCP23S17::set_device_address(uint8_t device_addr) {
  if (device_addr != 0) {
    this->device_opcode_ |= ((device_addr & 0b111) << 1);
  }
}

void MCP23S17::setup() {
  ESP_LOGCONFIG(TAG, "Setting up MCP23S17...");
  this->spi_setup();

  this->enable();
  uint8_t cmd = 0b01000000;
  this->transfer_byte(cmd);
  this->transfer_byte(0x18);
  this->transfer_byte(0x0A);
  this->transfer_byte(this->device_opcode_);
  this->transfer_byte(0);
  this->transfer_byte(0xFF);
  this->transfer_byte(0xFF);

  for (uint8_t i = 0; i < 20; i++) {
    this->transfer_byte(0);
  }
  this->disable();
}

void MCP23S17::dump_config() {
  ESP_LOGCONFIG(TAG, "MCP23S17:");
  LOG_PIN("  CS Pin: ", this->cs_);
}

float MCP23S17::get_setup_priority() const { return setup_priority::HARDWARE; }

bool MCP23S17::digital_read(uint8_t pin) {
  uint8_t bit = pin % 8;
  uint8_t reg_addr = pin < 8 ? MCP23S17_GPIOA : MCP23S17_GPIOB;
  uint8_t value = 0;
  this->read_reg(reg_addr, &value);
  return value & (1 << bit);
}

void MCP23S17::digital_write(uint8_t pin, bool value) {
  uint8_t reg_addr = pin < 8 ? MCP23S17_OLATA : MCP23S17_OLATB;
  this->update_reg(pin, value, reg_addr);
}

void MCP23S17::pin_mode(uint8_t pin, uint8_t mode) {
  uint8_t iodir = pin < 8 ? MCP23S17_IODIRA : MCP23S17_IODIRB;
  uint8_t gppu = pin < 8 ? MCP23S17_GPPUA : MCP23S17_GPPUB;
  switch (mode) {
    case MCP23S17_INPUT:
      this->update_reg(pin, true, iodir);
      break;
    case MCP23S17_INPUT_PULLUP:
      this->update_reg(pin, true, iodir);
      this->update_reg(pin, true, gppu);
      break;
    case MCP23S17_OUTPUT:
      this->update_reg(pin, false, iodir);
      break;
    default:
      break;
  }
}

void MCP23S17::update_reg(uint8_t pin, bool pin_value, uint8_t reg_addr) {
  uint8_t bit = pin % 8;
  uint8_t reg_value = 0;
  if (reg_addr == MCP23S17_OLATA) {
    reg_value = this->olat_a_;
  } else if (reg_addr == MCP23S17_OLATB) {
    reg_value = this->olat_b_;
  } else {
    this->read_reg(reg_addr, &reg_value);
  }

  if (pin_value)
    reg_value |= 1 << bit;
  else
    reg_value &= ~(1 << bit);

  this->write_reg(reg_addr, reg_value);

  if (reg_addr == MCP23S17_OLATA) {
    this->olat_a_ = reg_value;
  } else if (reg_addr == MCP23S17_OLATB) {
    this->olat_b_ = reg_value;
  }
}

bool MCP23S17::read_reg(uint8_t reg, uint8_t *value) {
  this->enable();
  this->transfer_byte(this->device_opcode_ | 1);
  this->transfer_byte(reg);
  *value = this->transfer_byte(0xFF);
  this->disable();
  return true;
}

bool MCP23S17::write_reg(uint8_t reg, uint8_t value) {
  this->enable();
  this->transfer_byte(this->device_opcode_);
  this->transfer_byte(reg);
  this->transfer_byte(value);

  this->disable();
  return true;
}

MCP23S17GPIOPin::MCP23S17GPIOPin(MCP23S17 *parent, uint8_t pin, uint8_t mode, bool inverted)
    : GPIOPin(pin, mode, inverted), parent_(parent) {}
void MCP23S17GPIOPin::setup() { this->pin_mode(this->mode_); }
void MCP23S17GPIOPin::pin_mode(uint8_t mode) { this->parent_->pin_mode(this->pin_, mode); }
bool MCP23S17GPIOPin::digital_read() { return this->parent_->digital_read(this->pin_) != this->inverted_; }
void MCP23S17GPIOPin::digital_write(bool value) { this->parent_->digital_write(this->pin_, value != this->inverted_); }

}  // namespace mcp23s17
}  // namespace esphome
