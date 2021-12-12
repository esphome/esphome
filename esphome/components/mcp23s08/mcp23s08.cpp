#include "mcp23s08.h"
#include "esphome/core/log.h"

namespace esphome {
namespace mcp23s08 {

static const char *const TAG = "mcp23s08";

void MCP23S08::set_device_address(uint8_t device_addr) {
  if (device_addr != 0) {
    this->device_opcode_ |= ((device_addr & 0x03) << 1);
  }
}

void MCP23S08::setup() {
  ESP_LOGCONFIG(TAG, "Setting up MCP23S08...");
  this->spi_setup();

  this->enable();
  uint8_t cmd = 0b01000000;
  this->transfer_byte(cmd);
  this->transfer_byte(mcp23x08_base::MCP23X08_IOCON);
  this->transfer_byte(0b00011000);  // Enable HAEN pins for addressing
  this->disable();

  if (this->open_drain_ints_) {
    // enable open-drain interrupt pins, 3.3V-safe
    this->write_reg(mcp23x08_base::MCP23X08_IOCON, 0x04);
  }
}

void MCP23S08::dump_config() {
  ESP_LOGCONFIG(TAG, "MCP23S08:");
  LOG_PIN("  CS Pin: ", this->cs_);
}

bool MCP23S08::read_reg(uint8_t reg, uint8_t *value) {
  this->enable();
  this->transfer_byte(this->device_opcode_ | 1);
  this->transfer_byte(reg);
  *value = this->transfer_byte(0);
  this->disable();
  return true;
}

bool MCP23S08::write_reg(uint8_t reg, uint8_t value) {
  this->enable();
  this->transfer_byte(this->device_opcode_);
  this->transfer_byte(reg);
  this->transfer_byte(value);
  this->disable();
  return true;
}

}  // namespace mcp23s08
}  // namespace esphome
