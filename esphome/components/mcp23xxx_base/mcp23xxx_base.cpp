#include "mcp23xxx_base.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace mcp23xxx_base {

template<uint8_t N> void MCP23XXXGPIOPin<N>::setup() {
  this->pin_mode(flags_);
  this->parent_->pin_interrupt_mode(this->pin_, this->interrupt_mode_);
}
template<uint8_t N> void MCP23XXXGPIOPin<N>::pin_mode(gpio::Flags flags) { this->parent_->pin_mode(this->pin_, flags); }
template<uint8_t N> bool MCP23XXXGPIOPin<N>::digital_read() {
  return this->parent_->digital_read(this->pin_) != this->inverted_;
}
template<uint8_t N> void MCP23XXXGPIOPin<N>::digital_write(bool value) {
  this->parent_->digital_write(this->pin_, value != this->inverted_);
}
template<uint8_t N> std::string MCP23XXXGPIOPin<N>::dump_summary() const {
  return str_snprintf("%u via MCP23XXX", 15, pin_);
}

template class MCP23XXXGPIOPin<8>;
template class MCP23XXXGPIOPin<16>;

}  // namespace mcp23xxx_base
}  // namespace esphome
