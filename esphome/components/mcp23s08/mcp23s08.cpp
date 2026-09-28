#include "mcp23s08.h"
#include "esphome/core/log.h"

namespace esphome::mcp23s08 {

static const char *const TAG = "mcp23s08";

// IOCON register bits
static constexpr uint8_t IOCON_SEQOP = 0x20;  // Sequential operation mode
static constexpr uint8_t IOCON_HAEN = 0x08;   // Hardware address enable
static constexpr uint8_t IOCON_ODR = 0x04;    // Open-drain output for INT pin

void MCP23S08::set_device_address(uint8_t device_addr) {
  if (device_addr != 0) {
    this->device_opcode_ |= ((device_addr & 0x03) << 1);
  }
}

void MCP23S08::setup() {
  this->spi_setup();

  // Enable HAEN (broadcast to all chips since HAEN isn't active yet)
  this->enable();
  this->transfer_byte(0b01000000);
  this->transfer_byte(mcp23x08_base::MCP23X08_IOCON);
  this->transfer_byte(IOCON_SEQOP | IOCON_HAEN);
  this->disable();

  // Read current output register state
  this->read_reg(mcp23x08_base::MCP23X08_OLAT, &this->olat_);

  if (this->open_drain_ints_) {
    // enable open-drain interrupt pins, 3.3V-safe (addressed, only this chip)
    this->write_reg(mcp23x08_base::MCP23X08_IOCON, IOCON_SEQOP | IOCON_HAEN | IOCON_ODR);
  }

  this->setup_interrupt_pin_();
}

void MCP23S08::dump_config() {
  ESP_LOGCONFIG(TAG, "MCP23S08:");
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_PIN("  Interrupt Pin: ", this->interrupt_pin_);
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

}  // namespace esphome::mcp23s08
