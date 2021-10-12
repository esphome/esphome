#include "mcp23xxx_base.h"
#include "esphome/core/log.h"

namespace esphome {
namespace mcp23xxx_base {

float MCP23XXXBase::get_setup_priority() const { return setup_priority::IO; }

void MCP23XXXGPIOPin::setup() { pin_mode(flags_); }
void MCP23XXXGPIOPin::pin_mode(gpio::Flags flags) { this->parent_->pin_mode(this->pin_, flags); }
bool MCP23XXXGPIOPin::digital_read() { return this->parent_->digital_read(this->pin_) != this->inverted_; }
void MCP23XXXGPIOPin::digital_write(bool value) { this->parent_->digital_write(this->pin_, value != this->inverted_); }
std::string MCP23XXXGPIOPin::dump_summary() const {
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "%u via MCP23XXX", pin_);
  return buffer;
}

}  // namespace mcp23xxx_base
}  // namespace esphome
