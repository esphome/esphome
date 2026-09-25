#include "mcp23s17.h"
#include "esphome/core/log.h"

namespace esphome::mcp23s17 {

static const char *const TAG = "mcp23s17";

// IOCON register bits
static constexpr uint8_t IOCON_SEQOP = 0x20;   // Sequential operation mode
static constexpr uint8_t IOCON_MIRROR = 0x40;  // Mirror INTA/INTB pins
static constexpr uint8_t IOCON_HAEN = 0x08;    // Hardware address enable
static constexpr uint8_t IOCON_ODR = 0x04;     // Open-drain output for INT pin

void MCP23S17::set_device_address(uint8_t device_addr) {
  if (device_addr != 0) {
    this->device_opcode_ |= ((device_addr & 0b111) << 1);
  }
}

void MCP23S17::setup() {
  this->spi_setup();

  // Enable HAEN (broadcast to addresses 0 and 4 since HAEN isn't active yet)
  this->enable();
  this->transfer_byte(0b01000000);
  this->transfer_byte(mcp23x17_base::MCP23X17_IOCONA);
  this->transfer_byte(IOCON_SEQOP | IOCON_HAEN);
  this->disable();

  this->enable();
  this->transfer_byte(0b01001000);
  this->transfer_byte(mcp23x17_base::MCP23X17_IOCONA);
  this->transfer_byte(IOCON_SEQOP | IOCON_HAEN);
  this->disable();

  // Read current output register state
  this->read_reg(mcp23x17_base::MCP23X17_OLATA, &this->olat_a_);
  this->read_reg(mcp23x17_base::MCP23X17_OLATB, &this->olat_b_);

  uint8_t iocon_flags = IOCON_SEQOP | IOCON_HAEN;
  if (this->open_drain_ints_) {
    iocon_flags |= IOCON_ODR;
  }
  if (this->interrupt_pin_ != nullptr) {
    // Mirror INTA/INTB so either pin fires for changes on any port
    iocon_flags |= IOCON_MIRROR;
  }
  if (this->open_drain_ints_ || this->interrupt_pin_ != nullptr) {
    this->write_reg(mcp23x17_base::MCP23X17_IOCONA, iocon_flags);
    this->write_reg(mcp23x17_base::MCP23X17_IOCONB, iocon_flags);
  }

  this->setup_interrupt_pin_();
}

void MCP23S17::dump_config() {
  ESP_LOGCONFIG(TAG, "MCP23S17:");
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_PIN("  Interrupt Pin: ", this->interrupt_pin_);
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

}  // namespace esphome::mcp23s17
