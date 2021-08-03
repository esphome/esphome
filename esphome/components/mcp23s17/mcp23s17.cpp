#include "mcp23s17.h"
#include "esphome/core/log.h"

namespace esphome {
namespace mcp23s17 {

static const char *const TAG = "mcp23s17";

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
  this->transfer_byte(mcp23x17_base::MCP23X17_IOCONA);
  this->transfer_byte(0b00011000);  // Enable HAEN pins for addressing
  this->disable();

  if (this->open_drain_ints_) {
    // enable open-drain interrupt pins, 3.3V-safe
    this->write_reg(mcp23x17_base::MCP23X17_IOCONA, 0x04);
    this->write_reg(mcp23x17_base::MCP23X17_IOCONB, 0x04);
  }
}

void MCP23S17::dump_config() {
  ESP_LOGCONFIG(TAG, "MCP23S17:");
  LOG_PIN("  CS Pin: ", this->cs_);
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

}  // namespace mcp23s17
}  // namespace esphome
