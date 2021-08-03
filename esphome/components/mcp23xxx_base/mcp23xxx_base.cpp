#include "mcp23xxx_base.h"
#include "esphome/core/log.h"

namespace esphome {
namespace mcp23xxx_base {

float MCP23XXXBase::get_setup_priority() const { return setup_priority::IO; }

MCP23XXXGPIOPin::MCP23XXXGPIOPin(MCP23XXXBase *parent, uint8_t pin, uint8_t mode, bool inverted,
                                 MCP23XXXInterruptMode interrupt_mode)
    : GPIOPin(pin, mode, inverted), parent_(parent), interrupt_mode_(interrupt_mode) {}
void MCP23XXXGPIOPin::setup() { this->pin_mode(this->mode_); }
void MCP23XXXGPIOPin::pin_mode(uint8_t mode) {
  this->parent_->pin_mode(this->pin_, mode);
  this->parent_->pin_interrupt_mode(this->pin_, this->interrupt_mode_);
}
bool MCP23XXXGPIOPin::digital_read() { return this->parent_->digital_read(this->pin_) != this->inverted_; }
void MCP23XXXGPIOPin::digital_write(bool value) { this->parent_->digital_write(this->pin_, value != this->inverted_); }

}  // namespace mcp23xxx_base
}  // namespace esphome
